#include "mysql.h"

#include <sys/stat.h>

// #include "../../build/_deps/catch2-src/src/catch2/internal/catch_enforce.hpp"

namespace m_sylar
{
static Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

MysqlAwaiter::MysqlAwaiter(MYSQL* mysql, const int status, const uint64_t timeOut){
    m_timeout = timeOut;
    m_fd = mysql_get_socket(mysql);
    if(status & MYSQL_WAIT_READ ){
        m_event = (FdContext::Event)(m_event | FdContext::Event::READ);
    }
    if(status & MYSQL_WAIT_WRITE){
        m_event = (FdContext::Event)(m_event | FdContext::Event::WRITE);
    }
}

MysqlAwaiter::~MysqlAwaiter(){
    IOManager::getInstance()->delEvent(m_fd, m_event);
}

void MysqlAwaiter::on_suspend() {
    const TimeManager::ptr tim = m_sylar::TimeManager::getInstance();
    auto tl_state = std::make_shared<TimeLimitInfo::State> ();
    // 回调事件注册
    tim->addEventWithTimeout(m_fd, m_event, [this, tl_state](){
        if(*tl_state == TimeLimitInfo::FINISHED) {
            resume(IOState::SUCCESS);
        }
        else if(*tl_state == TimeLimitInfo::TIMEOUT) {
            M_SYLAR_LOG_WARN(g_logger) << "mysqlAwaiter timed out";
            resume(IOState::TIMEOUT);
        }
        else {
            M_SYLAR_LOG_WARN(g_logger) << "mysqlAwaiter trigged with a bad IOState Type";
            resume(IOState::UNKNOWN);
        }
    }, m_timeout, tl_state, 1);
}

void MysqlAwaiter::before_resume() {
}



// 响应封装
MySQLResp::MySQLResp(MYSQL* mysql, IOState state) : m_mysql(mysql) {
    if(mysql == nullptr) {
        m_state = IOState::FAILED;          // 错误传参默认失败
        return;
    }
    m_state = state;
}

MySQLResp::~MySQLResp(){
    if(m_respBody) {
        mysql_free_result(m_respBody);
        m_respBody = nullptr;
    }
}


MySQLResp::Value::Value(char* data, int length){
    m_data = data;
    m_length = length;
}

MySQLResp::Value::Value(){
    m_isValid = false;
}

std::string MySQLResp::Value::get(){
    if(!m_isValid){
        M_SYLAR_LOG_WARN(g_logger) << "failed to get Value data, invalid Value ";
        return "";
    }
    return std::string(m_data, m_length);
}

char* MySQLResp::Value::get(size_t& length){
    if(!m_isValid){
        M_SYLAR_LOG_WARN(g_logger) << "failed to get Value data, invalid Value ";
        return nullptr;
    }
    length = m_length;
    return m_data;
}


MySQLResp::Row::Row(MYSQL_ROW row, int colCount, unsigned long* eleLength){
    m_colCount = colCount;
    m_colIdx = 0;
    m_row = row;
    m_eleLength = eleLength;
}

MySQLResp::Row::~Row() { }

MySQLResp::Value MySQLResp::Row::nextValue(){
    size_t length = 0;
    if(m_colIdx < m_colCount){
        length = m_eleLength[m_colIdx];
        return MySQLResp::Value(m_row[m_colIdx++], length);
    }
    else{
        length = -1;
        return MySQLResp::Value();
    }
}

MySQLResp::Row MySQLResp::nextRow(){
    if(m_state != IOState::SUCCESS || !m_respBody) {
        M_SYLAR_LOG_ERROR(g_logger) << "failed to get next row: MySQL response is invalid or null resp"; 
        return {nullptr, 0, nullptr};
    }
    auto row = mysql_fetch_row(m_respBody);
    auto len = mysql_fetch_lengths(m_respBody);

    return {row, m_colCount, len};
}


void MySQLResp::resetRow() {
    if(m_state != IOState::SUCCESS  || !m_respBody) {
        M_SYLAR_LOG_ERROR(g_logger) << "failed to get next row: MySQL response is invalid null resp"; 
        return;
    }
    mysql_data_seek(m_respBody, 0);
    return; 
}

int MySQLResp::formatDate() {
    if(m_state != IOState::SUCCESS  || !m_respBody) {
        M_SYLAR_LOG_ERROR(g_logger) << "failed to formatDate: MySQL response is invalid or null resp"; 
        return -1;
    }
    std::vector<std::vector<std::pair<char*, size_t>>*> map;
    map.resize(m_colCount);
    for(int i = 0; i < m_colCount; i++) {
        MYSQL_FIELD* field = mysql_fetch_field(m_respBody);
        std::string col_name = std::string(field->name, field->name_length);
        m_respMapData[col_name] = {};
        map[i] = &m_respMapData[col_name];  // 第一维为列，第二维值
        map[i]->resize(m_rowCount);
    }

    // 数据填充
    if(m_respBody) {
        auto row = nextRow();
        for(int row_idx = 0; row; (row_idx++, row = nextRow())) {
            auto col = row.nextValue();
            for(int col_idx = 0; col; (col_idx++, col = row.nextValue())) {
                size_t length = 0;
                char* data = col.get(length);
                (*map[col_idx])[row_idx] = std::pair<char*, size_t> (data, length);
            }
        }
        resetRow();
    }
    else {
        M_SYLAR_LOG_ERROR(g_logger) << "failed to formatDate: MySQL response is invalid or null resp"; 
        return -1;
    }
    return 0;
}

std::string MySQLResp::ColProxy::operator[](int i) {
    M_SYLAR_ASSERT2(m_state && m_vec && i >= 0 && i < m_vec->size(), "index out of range");
    return std::string((*m_vec)[i].first, (*m_vec)[i].second); 
}

MySQLResp::ColProxy MySQLResp::operator[](std::string fieldName) {
    auto iterat = m_respMapData.find(fieldName);
    if(iterat == m_respMapData.end()) {
        M_SYLAR_LOG_ERROR(g_logger) << "can not find the colume named " << fieldName;
        return ColProxy();
    }
    else {
        return ColProxy(iterat->second);
    }
}


Task<IOState> MySQLResp::co_fetchAll() {
    if (m_state != IOState::SUCCESS) {co_return m_state; }

    // 获取所有数据,加载到内存
    //      初始化异步循环
    int status = mysql_store_result_start(&m_respBody, m_mysql);

    //      循环等待
    while(status) {
        constexpr uint64_t timeout = MYSQL_QUERY_TIMEOUT;
        const IOState state = co_await MysqlAwaiter(m_mysql, status, timeout);
        // M_SYLAR_LOG_DEBUG(g_logger) << "origin state: " << state;
        if(state == IOState::TIMEOUT) {
            m_state = IOState::FAILED;
            M_SYLAR_LOG_WARN(g_logger) << "fetch data from mysql timed out";
            co_return IOState::TIMEOUT;
        }
        status = mysql_store_result_cont(&m_respBody, m_mysql, status);
    }


    // 状态变量获取 (列数,行数等)
    if(!m_respBody) {
        // update等语句判断
        if (mysql_field_count(m_mysql) != 0) {
            m_state = IOState::FAILED;
            M_SYLAR_LOG_ERROR(g_logger) << std::string("mysql_store_result failed, error: ") + mysql_error(m_mysql);
            co_return IOState::FAILED;
        }
        // 没有结果集的语句(比如 UPDATE),这里可以按需要单独处理,而不是当成 FAILED
        m_colCount = 0;
        m_rowCount = 0;
        m_state = IOState::SUCCESS;
        co_return IOState::SUCCESS;
    }
    // 普通查询判断
    m_colCount = mysql_field_count(m_mysql);
    m_rowCount = mysql_num_rows(m_respBody);
    if(!m_respBody || !m_colCount) {
        m_state = IOState::FAILED;
        co_return IOState::FAILED;
    }

    m_state = IOState::SUCCESS;
    co_return IOState::SUCCESS;
}








// MYSQL 封装
MySQLConn::MySQLConn(){
    m_mysql = mysql_init(nullptr);
    if(m_mysql == nullptr) {
        M_SYLAR_LOG_ERROR(g_logger) << "failed to init a mysql instance";
        throw std::runtime_error("failed to init a mysql instance");
    }
    m_state = INIT;
}

MySQLConn::~MySQLConn(){
    if(m_state != INIT && m_mysql) {
        mysql_close(m_mysql);
    }
}

int MySQLConn::connect(ConnectInfoBase& info) {
    // 状态同步
    if (mysql_options(m_mysql, MYSQL_OPT_NONBLOCK, nullptr)) {
        M_SYLAR_LOG_ERROR(g_logger) << "mysql_options failed : " << mysql_error(m_mysql) << std::endl;
    }

    // 获取连接信息并连接
    const auto& mysql_info = dynamic_cast<MySQLConnectInfo&>(info);
    const MYSQL* rt = mysql_real_connect(m_mysql, mysql_info.host.c_str(), mysql_info.user.c_str(), mysql_info.passwd.c_str(),
                                    mysql_info.db.c_str(), mysql_info.port, nullptr, mysql_info.clientflag);
    if(rt == nullptr) {
        M_SYLAR_LOG_ERROR(g_logger) << "connect failed, error: " << mysql_error(m_mysql);
        return -1;
    }

    // 类内状态处理->ready
    m_state = State::READY;
    return 0;
}


Task<MySQLResp::ptr> MySQLConn::executeQuery(const std::string& query){
    int err = 0;
    // if (mysql_options(m_mysql, MYSQL_OPT_NONBLOCK, 0)) {
    //     std::cout << "mysql_optins failed : " << mysql_error(m_mysql) << std::endl;
    // }
    int status = mysql_real_query_start(&err, m_mysql, query.c_str(), query.length());
    
    if(status == 0) {
        co_return std::make_shared<MySQLResp>(m_mysql, IOState::SUCCESS);
    }

    if(err) {
        std::cout << std::string("mysql_real_query_start failed, error : ") + mysql_error(m_mysql) << std::endl;
        std::string errinfo = std::string("mysql_real_query_start failed, error : ") + mysql_error(m_mysql);
        co_return std::make_shared<MySQLResp>(m_mysql, IOState::FAILED);
    }


    while(status) {
        uint64_t timeout = MYSQL_QUERY_TIMEOUT;
        IOState state = co_await MysqlAwaiter(m_mysql, status, timeout);
        // M_SYLAR_LOG_DEBUG(g_logger) << "origin state: " << state;
        if(state == IOState::TIMEOUT) {
            co_return std::make_shared<MySQLResp>(m_mysql, IOState::TIMEOUT);
        }
        status = mysql_real_query_cont(&err, m_mysql, status);
        if(err) {
            std::string errinfo = std::string("mysql_real_query_start failed, error : ") + mysql_error(m_mysql);
            co_return std::make_shared<MySQLResp>(m_mysql, IOState::FAILED);
        }
    }

    co_return std::make_shared<MySQLResp>(m_mysql, IOState::SUCCESS);
}






// Manager 用于连接满时协程异步等待
class MySQLGetConnAwaiter : public Awaiter<void>{
public: 
    explicit MySQLGetConnAwaiter(MySQLPoolManager* MySQLMgr) {
        if(MySQLMgr == nullptr) {
            M_SYLAR_LOG_ERROR(g_logger) << "MySQLMgr is nullptr, failed to await task";
        }
        m_mgr = MySQLMgr;
    }
    ~MySQLGetConnAwaiter() override = default;

protected:
    void on_suspend() override {
        if(!m_mgr) {throw std::runtime_error("MySQLMgr is nullptr, failed to await task");}

        m_mgr->registeConnCb([this](){
            resume();
        });
    }

    void before_resume() override {
    }

private:
    MySQLPoolManager* m_mgr;
};



MySQLPoolManager::MySQLPoolManager(int min_conn, int max_conn)
    : m_sylar::DBPool<MySQLConn, MySQLResp>(min_conn, max_conn) {

}



Task<MySQLResp::ptr> MySQLPoolManager::executeQuery(const std::string& query) {
    if(!checkRunState()) {
        co_return std::make_shared<MySQLResp>(nullptr, IOState::FAILED);
    }

    // 获取一个连接
    ConnectWrapper<MySQLConn, MySQLResp>::ptr connect_wrapper = borrowOneConn();

    // 获取连接并执行
retry:
    if(connect_wrapper->getConnIdx() >= 0) {        // 有当前可用连接std::string finishQuery = "RESET SESSION;";
        // std::string finishQuery =   "SET @@session.autocommit = 1; SET @@session.transaction_isolation = 'REPEATABLE-READ';RESET SESSION;"

        MySQLResp::ptr resp = co_await connect_wrapper->getConnector()->executeQuery(query);
        if(resp->getState() != IOState::SUCCESS) {
            M_SYLAR_LOG_ERROR(g_logger) << "failed to execute query: " << query << " error: " << mysql_error(connect_wrapper->getConnector()->getMYSQL());
        }
        resp->co_fetchAll();

        // 回滚所有未提交事务
        const MySQLResp::ptr resetPtr = co_await connect_wrapper->getConnector()->executeQuery("ROLLBACK;");
        if(resetPtr->getState() != IOState::SUCCESS) {
            M_SYLAR_LOG_ERROR(g_logger) << "failed to execute query: ROLLBACK; error: " << mysql_error(connect_wrapper->getConnector()->getMYSQL());
        }
        resetPtr->co_fetchAll();
        bool isTimo = (resp->getState() == IOState::TIMEOUT || resetPtr->getState() == IOState::TIMEOUT);
        connect_wrapper.reset();
        co_return resp;
    }
    else {  // 正繁忙，无空闲
        if(m_maxConnector > m_connectorCount) {
            // 扩列
            expand();
        }
        else {  
            // 等待
            co_await MySQLGetConnAwaiter(this);
            if(!checkRunState()) {      // 不在运行，
                co_return std::make_shared<MySQLResp>(nullptr, IOState::FAILED);
            }
        }
        goto retry;
    }
    M_SYLAR_LOG_ERROR(g_logger) << "bad code branch";
    co_return std::make_shared<MySQLResp>(nullptr, IOState::FAILED);
}


int MySQLPoolManager::init(const std::string& host,
                            const std::string& user,
                            const std::string& passwd,
                            const std::string& db,
                            const unsigned int port,
                            const unsigned long client_flag) {
    if(m_state != INIT) {
        m_state = ERROR;
    }   
    // 信息记录
    m_connectorBaseInfo.host = host;
    m_connectorBaseInfo.user = user;
    m_connectorBaseInfo.passwd = passwd;
    m_connectorBaseInfo.db = db;
    m_connectorBaseInfo.port = port;
    m_connectorBaseInfo.clientflag = client_flag;

    // 连接
    int rt = 0;
    for(int i = 0; i < m_minConnector && !rt; i++) {
        rt = m_connectors[i]->connect(m_connectorBaseInfo);
        m_freeConnInfos.push_back({i});
    }
    if(rt) {    // 错误检查
        m_state = ERROR;
        M_SYLAR_LOG_ERROR(g_logger) << "failed to init MySQLPoolManager, connect failed.";
        return -1;
    }
    m_connectorCount = m_minConnector;
    m_state = READY;
    return 0;
}



int MySQLPoolManager::registeConnCb(std::function<void()> cb) {
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


int MySQLPoolManager::tickle() {
    if(m_waitConnCb.empty()) {return 0;}

    std::list<std::function<void()>> temp_tasks;

    std::unique_lock<std::shared_mutex> w_lock(m_ConnectPoolMutex);
    if(m_state == MySQLPoolManager::CLOSING) {
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
}
