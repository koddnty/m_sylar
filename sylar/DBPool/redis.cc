#include <hiredis/read.h>
#include <memory>
#include <hiredis/hiredis.h>
#include "redis.h"
#include "basic/macro.h"
#include "database.h"


namespace m_sylar {

static Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");


// Redis响应封装
RedisResp::RedisResp(redisReply* reply, IOState::State state, bool is_child){
    M_SYLAR_ASSERT2(reply, "reply is nullptr");
    m_reply = reply;
    m_is_child = is_child;
    m_state = state;
}

RedisResp::~RedisResp(){
    if(!m_is_child) {
        freeReplyObject(m_reply);
    }
}

int RedisResp::getType(){
    return m_reply->type;
}

long long RedisResp::asInt(){
    if(m_reply->type == REDIS_REPLY_INTEGER) {
        return m_reply->integer;
    }
    else {
        throw std::bad_cast();
    }
}

double RedisResp::asDouble() {
    if(m_reply->type == REDIS_REPLY_DOUBLE) {
        return m_reply->dval;
    }
    else if(m_reply->type == REDIS_REPLY_INTEGER) {
        return m_reply->integer;
    }
    else {
        throw std::bad_cast();
    }
}

std::string RedisResp::asString(){
    switch (m_reply->type) {
        case REDIS_REPLY_ERROR:
        case REDIS_REPLY_STRING:
        case REDIS_REPLY_VERB:
        case REDIS_REPLY_DOUBLE:
        case REDIS_REPLY_STATUS:
        case REDIS_REPLY_BIGNUM:
            return std::string(m_reply->str, m_reply->len);
            break;
        case REDIS_REPLY_INTEGER:
            return std::to_string(m_reply->integer);
            break;
        default:
            std::cout << m_reply->type << std::endl;
            throw std::bad_cast();
            break;
    }
}

const std::vector<RedisResp::ptr>& RedisResp::asArray() {
    if(m_is_arry_inited) {
        return m_array;
    }

    m_array.resize(m_reply->elements);
    for(int i = 0; i < m_reply->elements; i++) {
        m_array[i] = std::make_shared<RedisResp>(m_reply->element[i], IOState::SUCCESS, true);        // 析构不释放。
    }
    m_is_arry_inited = true;

    return m_array;
}


// redis连接封装
RedisConn::RedisConn() {
}

RedisConn::~RedisConn() {
    if(m_connect) {
        redisFree(m_connect);
    }
}

int RedisConn::connect(const std::string& ip, int port) {
    m_connect = redisConnect(ip.c_str(), port);
    if(m_connect == nullptr || m_connect->err) {
        M_SYLAR_LOG_ERROR(g_logger) << "failed to connect to redis, error: " << (m_connect ? m_connect->errstr : "null");
        return -1;
    }
    return 0;
}

RedisResp::ptr RedisConn::executeQuery(const std::string& query) {
    M_SYLAR_ASSERT2(m_connect != nullptr, "redis context is nullptr");
    redisReply* reply = (redisReply*)redisCommand(m_connect, query.c_str());
    if(reply == nullptr) {
        M_SYLAR_LOG_ERROR(g_logger) << "failed to execute redis query, error: " << m_connect->errstr;
        return nullptr; 
    }
    return std::make_shared<RedisResp>(reply);
}


// redis连接池管理


// Manager 用于连接满时协程异步等待
class RedisGetConnAwaiter : public Awaiter<void>{
public: 
    RedisGetConnAwaiter(RedisPoolManager* MySQLMgr) {
        if(MySQLMgr == nullptr) {
            M_SYLAR_LOG_ERROR(g_logger) << "MySQLMgr is nullptr, failed to await task";
        }
        m_mgr = MySQLMgr;
    }
    ~RedisGetConnAwaiter() {}

    void on_suspend()override{
        if(!m_mgr) {throw std::runtime_error("MySQLMgr is nullptr, failed to await task");}

        m_mgr->registeConnCb([this](){
            resume();
        });
    }

    void before_resume() override {
    }

private:
    RedisPoolManager* m_mgr;
};




RedisPoolManager::RedisPoolManager(int min_conn, int max_conn) 
    : m_sylar::DBPool<RedisConn, RedisResp>(min_conn, max_conn) {
}

RedisPoolManager::~RedisPoolManager() {
    
}

int RedisPoolManager::init(const std::string& ip, int port) {
    // 信息记录
    m_connectInfo.ip = ip;
    m_connectInfo.port = port;

    // 连接
    int rt = 0;
    for(int i = 0; i < m_minConnector && !rt; i++) {
        rt = m_connectors[i]->connect(ip, port);
        m_freeConnInfos.push_back({i});
    }
    if(rt) {    // 错误检查
        m_state = ERROR;
        M_SYLAR_LOG_ERROR(g_logger) << "failed to init RedisManagePool, connect failed.";
        return -1;
    }
    m_connectorCount = m_minConnector;
    m_state = READY;
    return 0;
}

Task<std::shared_ptr<RedisResp>> RedisPoolManager::executeQuery(const std::string& query) {
    if(!checkRunState()) {
        co_return std::make_shared<RedisResp>(nullptr, IOState::FAILED);
    }

    // 获取一个连接
    int connectorIdx = -1;

    // 获取连接并执行
retry:
    connectorIdx = borrowOneConn();
    if(connectorIdx >= 0) {        // 有当前可用连接std::string finishQuery = "RESET SESSION;";
        // std::string finishQuery =   "SET @@session.autocommit = 1; SET @@session.transaction_isolation = 'REPEATABLE-READ';RESET SESSION;"
        std::string finishQuery = "RESET SESSION;";
        RedisResp::ptr resp =  m_connectors[connectorIdx]->executeQuery(query);
        RedisResp::ptr resetPtr =  m_connectors[connectorIdx]->executeQuery(finishQuery);
        bool isTimo = (resp->getState() == IOState::TIMEOUT || resetPtr->getState() == IOState::TIMEOUT);
        if(-1 == returnConnn(connectorIdx, isTimo)) {   // 归还
            M_SYLAR_LOG_ERROR(g_logger) << "failed to return connect source";
        }    

        co_return resp;
    }
    else {  // 正繁忙，无空闲
        if(m_maxConnector > m_connectorCount) {
            // 扩列
            expand();
        }
        else {  
            // 等待
            co_await RedisGetConnAwaiter(this);
            if(!checkRunState()) {      // 不在运行，
                co_return std::make_shared<RedisResp>(nullptr, IOState::FAILED);
            }
        }
        goto retry;
    }
    M_SYLAR_LOG_ERROR(g_logger) << "bad code branch";
    co_return std::make_shared<RedisResp>(nullptr, IOState::FAILED);

}

int RedisPoolManager::registeConnCb(std::function<void()> cb) {
    if(!checkRunState()) {
        M_SYLAR_LOG_WARN(g_logger) << "failed to registe Connect callback, connect pool is not running";
        return -1;
    }
    if(!cb) {
        M_SYLAR_LOG_ERROR(g_logger) << "failed to registe Connect callback, parameter cn is null";
        return -1;
    }
    std::unique_lock<std::shared_mutex> w_lock(m_ConnectPoolMutex);
    if(m_state != FULL) { // 直接运行
        IOManager::getInstance()->schedule(TaskCoro20::create_func(cb));
    }
    else {
        m_waitConnCb.push_back(cb);
    }
    return 0;
}

int RedisPoolManager::tickle() {
    if(m_waitConnCb.empty()) {return 0;}

    std::list<std::function<void()>> temp_tasks;

    std::unique_lock<std::shared_mutex> w_lock(m_ConnectPoolMutex);
    if(m_state == RedisPoolManager::CLOSING) {
        temp_tasks.splice(temp_tasks.begin(), m_waitConnCb, m_waitConnCb.begin(), m_waitConnCb.end());
    }
    else { 
        auto task_begin = m_waitConnCb.begin();
        auto task_final = std::next(task_begin, std::min(m_freeConnInfos.size(), m_waitConnCb.size()) );
        temp_tasks.splice(temp_tasks.begin(), m_waitConnCb, task_begin, task_final);
    }
    w_lock.unlock();

    for(auto it : temp_tasks) {
        IOManager::getInstance()->schedule(TaskCoro20::create_func(it));            // 可行否？<...>
    }
    return 0;
}

int RedisPoolManager::borrowOneConn() {
    std::unique_lock<std::shared_mutex> w_lock(m_ConnectPoolMutex);
    if(!checkRunState() || m_state == RedisPoolManager::FULL) {
        return -1;
    }
    if(m_state == READY) {
        // 获取资源序号
        if(m_freeConnInfos.size() == 0) {
            M_SYLAR_LOG_ERROR(g_logger) << "state is not consistent with freeConnInfos[]";
            return -1;
        }
        int connectorIdx = m_freeConnInfos.front();
        m_freeConnInfos.pop_front();
        M_SYLAR_ASSERT2(connectorIdx >= 0 && connectorIdx < m_connectorCount, "connector is out of range");
        m_busyConnCount++;
        if(m_busyConnCount == m_connectorCount) {
            m_state = FULL;
        }
        return connectorIdx;
    }
    M_SYLAR_LOG_WARN(g_logger) << "unknown DBManager State: " << m_state;
    return -1;

}

int RedisPoolManager::returnConnn(int free_idx, bool isTimeOut) {
    std::unique_lock<std::shared_mutex> w_lock(m_ConnectPoolMutex);
    if(!checkRunState() || (free_idx < 0 || free_idx >= m_maxConnector)) {
        return -1;
    }

    if(isTimeOut) {
        w_lock.unlock();
        RedisConn::ptr new_connect = std::make_shared<RedisConn>();
        int rt = new_connect->connect(m_connectInfo.ip, m_connectInfo.port);
        if(rt == -1) {
            M_SYLAR_LOG_ERROR(g_logger) << "failed to connect to database: " << m_connectInfo.ip;
            m_connectorCount--;
            m_busyConnCount--;
            return -1;
        }
        m_connectors[free_idx] = new_connect;
        w_lock.lock();
    }
    // 资源归还
    m_freeConnInfos.push_back({free_idx});
    m_busyConnCount--;
    if(m_state == FULL) {
        m_state = READY;
    }

    // 唤醒部分等待者
    w_lock.unlock();
    tickle();
    // std::cout << "\n pool state: free:" << m_freeConnInfos.size();
    return 0;
}

int RedisPoolManager::expand() {
    if(!checkRunState()) {
        M_SYLAR_LOG_ERROR(g_logger) << "failed to expand connector pool, invalid pool state";
    }

    if(m_maxConnector == m_connectorCount) {
        M_SYLAR_ASSERT2(m_connectorCount == m_connectors.size(), "connector count is not equals to connectors.size(), bad State");
        return 0;   // 已到极限
    }
    else {
        std::unique_lock<std::shared_mutex> w_lock(m_ConnectPoolMutex);
        // 计算增量
        int increaseNum = REDIS_CONNECT_CREASE_SPEED;
        int temp = m_maxConnector - m_connectorCount;
        increaseNum =  (increaseNum < temp) ? increaseNum : temp;

        // 扩容
        int rt = 0;
        int max_idx = 0;
        for(int i = m_connectorCount; i < m_connectorCount + increaseNum && !rt; i++) {
            rt = m_connectors[i]->connect(m_connectInfo.ip, m_connectInfo.port);
            max_idx = i;
            m_freeConnInfos.push_back({i});
        }
        if(rt) {    // 错误检查.
            M_SYLAR_LOG_ERROR(g_logger) << "failed to expand MySQLPoolManager, new connect failed.";
            m_connectorCount = max_idx;
            return -1;
        }

        // 信息设置
        m_connectorCount = m_connectorCount + increaseNum;
        if(m_connectorCount > m_busyConnCount) {
            m_state = RedisPoolManager::READY;
        }
        return 0;
    }
}


}