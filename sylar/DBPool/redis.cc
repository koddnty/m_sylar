#include <hiredis/read.h>
#include <memory>
#include <hiredis/hiredis.h>
#include "redis.h"
#include "basic/macro.h"
#include "database.hpp"


namespace m_sylar {

static Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");


// Redis响应封装
RedisResp::RedisResp(redisReply* reply, IOState state, bool is_child){
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

int RedisConn::connect(ConnectInfoBase::ptr info) {
    const auto conn_info = std::dynamic_pointer_cast<RedisConnectInfo>(info);
    m_connect = redisConnect(conn_info->host.c_str(), conn_info->port);
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



RedisPoolManager::RedisPoolManager(int min_conn, int max_conn)
    : m_sylar::DBPool<RedisConn, RedisResp>(min_conn, max_conn) {
}

RedisPoolManager::~RedisPoolManager() {
    
}

int RedisPoolManager::init(const std::string& host, const int port) {
    // 信息记录
    m_connectorBaseInfo = std::make_shared<RedisConnectInfo>();
    m_connectorBaseInfo->host = host;
    m_connectorBaseInfo->port = port;

    // 连接
    int rt = 0;
    for(int i = 0; i < m_minConnector && !rt; i++) {
        rt = m_connectors[i]->connect(m_connectorBaseInfo);
        m_freeConnInfos.push_back({i});
    }
    if(rt) {    // 错误检查
        m_state = ERROR;
        M_SYLAR_LOG_ERROR(g_logger) << "failed to init RedisManagePool, connect failed.";
        return -1;
    }
    m_connectorCount = m_minConnector.load();
    m_state = READY;
    return 0;
}

Task<std::shared_ptr<RedisResp>> RedisPoolManager::executeQuery(const std::string& query) {
    if(!checkRunState()) {
        co_return std::make_shared<RedisResp>(nullptr, IOState::FAILED);
    }


retry:
    // 获取一个连接
    ConnectWrapper<RedisConn, RedisResp>::ptr connect_wrapper = borrowOneConn();
    int conn_idx = connect_wrapper->getConnIdx();
    // 执行
    if(conn_idx >= 0) {        // 有当前可用连接std::string finishQuery = "RESET SESSION;";
        // std::string finishQuery =   "SET @@session.autocommit = 1; SET @@session.transaction_isolation = 'REPEATABLE-READ';RESET SESSION;"
        RedisResp::ptr resp =  m_connectors[conn_idx]->executeQuery(query);
        connect_wrapper.reset();
        conn_idx = -1;

        co_return resp;
    }
    else {  // 正繁忙，无空闲
        if(m_maxConnector > m_connectorCount) {
            // 扩列
            expand();
        }
        else {  
            // 等待
            co_await GetConnAwaiter(this);
            if(!checkRunState()) {      // 不在运行，
                co_return std::make_shared<RedisResp>(nullptr, IOState::FAILED);
            }
        }
        goto retry;
    }
    M_SYLAR_LOG_ERROR(g_logger) << "bad code branch";
    co_return std::make_shared<RedisResp>(nullptr, IOState::FAILED);
}


}