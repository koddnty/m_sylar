#include "mysql.h"

namespace m_sylar
{
static Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

MysqlAwaiter::MysqlAwaiter(MYSQL* mysql, int status){
    m_fd = mysql_get_socket(mysql);
    if(status & MYSQL_WAIT_READ ){
        m_event = (FdContext::Event)(m_event | FdContext::Event::READ);
    }
    if(status & MYSQL_WAIT_WRITE){
        m_event = (FdContext::Event)(m_event | FdContext::Event::WRITE);
    }
}

MysqlAwaiter::~MysqlAwaiter(){
    // std::cout << "registe  del Event finished" << std::endl;
    IOManager::getInstance()->delEvent(m_fd, m_event);
    // std::cout << "registe  del Event finished" << std::endl;
}

void MysqlAwaiter::on_suspend() {
    // auto self = shared_from_this();
    uint64_t timeOut = 5000000; // 5s
    TimeManager* tim = m_sylar::TimeManager::getInstance();
    std::shared_ptr<TimeLimitInfo::State> state = std::make_shared<TimeLimitInfo::State> ();
    // 回调事件注册
    tim->addEventWithTimeout(m_fd, m_event, [this, state](){  
        if(*state == TimeLimitInfo::FINISHED) {
            resume(IOState::SUCCESS);
        }
        else if(*state == TimeLimitInfo::TIMEOUT) {
            M_SYLAR_LOG_WARN(g_logger) << "mysqlAwaiter timed out";
            resume(IOState::TIMEOUT);
        }
        else {
            M_SYLAR_LOG_WARN(g_logger) << "mysqlAwaiter trigged with a bad IOState Type";
            resume(IOState::UNKNOWN);
        }
    }, timeOut, state, 1);
}

void MysqlAwaiter::before_resume() {
}



// 响应封装
MySQLResp::MySQLResp(MYSQL* mysql, IOState::State state){
    m_respBody = nullptr;
    m_colCount = 0;
    m_state = false;
    m_IOstate = state;
    if(mysql == nullptr) {
        return;
    }
    m_respBody = mysql_store_result(mysql);
    if(!m_respBody) {return; }
    m_colCount = mysql_field_count(mysql);
    m_rowCount = mysql_num_rows(m_respBody);
    if(!m_respBody || !m_colCount) {
        return;
    }
    m_state = true;
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
    if(!m_state || !m_respBody) {
        M_SYLAR_LOG_ERROR(g_logger) << "failed to get next row: MySQL response is invalid or null resp"; 
        return {nullptr, 0, nullptr};
    }
    auto row = mysql_fetch_row(m_respBody);
    auto len = mysql_fetch_lengths(m_respBody);
    return {row, m_colCount, len};
}


void MySQLResp::resetRow() {
    if(!m_state || !m_respBody) {
        M_SYLAR_LOG_ERROR(g_logger) << "failed to get next row: MySQL response is invalid null resp"; 
        return;
    }
    mysql_data_seek(m_respBody, 0);
    return; 
}

int MySQLResp::formatDate() {
    if(!m_state || !m_respBody) {
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









// MYSQL 封装
MySQLConn::MySQLConn(){
    m_mysql = mysql_init(nullptr);
    if(m_mysql == nullptr) {
        M_SYLAR_LOG_ERROR(g_logger) << "failed to init a mysql instence";
        throw std::runtime_error("failed to init a mysql instence");
    }
    m_state = State::INIT;
}

MySQLConn::~MySQLConn(){
    if(m_state != State::INIT && m_mysql) {
        // std::cout << "mysql 析构， fd = " << mysql_get_socket(m_mysql) << std::endl;
        // IOManager::getInstance()->delEvent( mysql_get_socket(m_mysql), (FdContext::Event)(FdContext::Event::READ | FdContext::Event::WRITE));
        // IOManager::getInstance()->closeWithNoClose( mysql_get_socket(m_mysql));
        mysql_close(m_mysql);
    }
}

int MySQLConn::connect(const std::string& host,
                        const std::string& user,
                        const std::string& passwd,
                        const std::string& db,
                        unsigned int port,
                        unsigned long clientflag) {

    // 状态同步
    if (mysql_options(m_mysql, MYSQL_OPT_NONBLOCK, 0)) {
        std::cout << "1mysql_optins failed : " << mysql_error(m_mysql) << std::endl;
    }

    MYSQL* rt = mysql_real_connect(m_mysql, host.c_str(), user.c_str(), passwd.c_str(), 
                                    db.c_str(), port, NULL, clientflag);
    if(rt == nullptr) {
        M_SYLAR_LOG_ERROR(g_logger) << "connect failed, error: " << mysql_error(m_mysql);
        return -1;
    }


    // m_mysql = rt;
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
        co_return std::make_shared<MySQLResp>(m_mysql);
    }

    if(err) {
        std::cout << std::string("mysql_real_query_start failed, error : ") + mysql_error(m_mysql) << std::endl;
        std::string errinfo = std::string("mysql_real_query_start failed, error : ") + mysql_error(m_mysql);
        co_return std::make_shared<MySQLResp>(m_mysql, IOState::FAILED);
    }


    while(status){
        IOState::State state = co_await MysqlAwaiter(m_mysql, status);
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

    co_return std::make_shared<MySQLResp>(m_mysql);
}






// Manager 用于连接满时协程异步等待
class MySQLGetConnAwaiter : public Awaiter<void>{
public: 
    MySQLGetConnAwaiter(MySQLPoolManager* MySQLMgr) {
        if(MySQLMgr == nullptr) {
            M_SYLAR_LOG_ERROR(g_logger) << "MySQLMgr is nullptr, failed to await task";
        }
        m_mgr = MySQLMgr;
    }
    ~MySQLGetConnAwaiter() {}

    void on_suspend()override{
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



MySQLPoolManager::MySQLPoolManager(int min_conn, int max_conn) {
    m_state = INIT;
    if(min_conn < 0 || max_conn < 0 || max_conn < min_conn) {
        throw std::runtime_error("failed to create a msyql db connector manager, min_conn or max_conn invalid");
    }
    m_connectors.resize(max_conn);
    for(int i = 0; i < max_conn; i++) {
        m_connectors[i] = std::make_shared<MySQLConn>();
    }
    m_minConnector = min_conn;
    m_maxConnector = max_conn;
}

MySQLPoolManager::~MySQLPoolManager() {
}

Task<MySQLResp::ptr> MySQLPoolManager::executeQuery(const std::string& query) {
    if(!checkRunState()) {
        co_return std::make_shared<MySQLResp>(nullptr, IOState::FAILED);
    }

    // 获取一个连接
    int connectorIdx = -1;

    // 获取连接并执行
retry:
    connectorIdx = borrowOneConn();
    if(connectorIdx >= 0) {        // 有当前可用连接
        MySQLResp::ptr resp = co_await m_connectors[connectorIdx]->executeQuery(query);
        if(-1 == returnConnn(connectorIdx)) {   // 归还
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
                                    unsigned int port,
                                    unsigned long clientflag) {
    if(m_state != INIT) {
        m_state = ERROR;
    }   
    // 信息记录
    m_connectorBaseInfo.host = host;
    m_connectorBaseInfo.user = user;
    m_connectorBaseInfo.passwd = passwd;
    m_connectorBaseInfo.db = db;
    m_connectorBaseInfo.port = port;
    m_connectorBaseInfo.clientflag = clientflag;

    // 连接
    int rt = 0;
    for(int i = 0; i < m_minConnector && !rt; i++) {
        rt = m_connectors[i]->connect(host, user, passwd, db, port, clientflag);
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

void MySQLPoolManager::close() {
    m_state = CLOSING;
    tickle();       // 唤醒所有回调
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

bool MySQLPoolManager::checkRunState() {
    switch(m_state) {
        case INIT:
        case CLOSED:
        case CLOSING:
        case ERROR:
            return false;
        case READY:
        case FULL:
            return true;
    }
    M_SYLAR_LOG_WARN(g_logger) << "failed to check running state, unknown State";
    return false;
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

int MySQLPoolManager::borrowOneConn(){
    std::unique_lock<std::shared_mutex> w_lock(m_ConnectPoolMutex);
    if(!checkRunState() || m_state == MySQLPoolManager::FULL) {
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

int MySQLPoolManager::returnConnn(int free_idx){
    std::unique_lock<std::shared_mutex> w_lock(m_ConnectPoolMutex);
    if(!checkRunState() || (free_idx < 0 || free_idx >= m_maxConnector)) {
        return -1;
    }
    // 资源归还
    m_freeConnInfos.push_back({free_idx});
    m_busyConnCount--;
    if(m_state == FULL) {
        m_state = READY;
    }
    if(-1 == mysql_reset_connection(m_connectors[free_idx]->getMYSQL())) {
        const char* error = mysql_error(m_connectors[free_idx]->getMYSQL());
        M_SYLAR_LOG_ERROR(g_logger) << "failed to reset a connection, error: " << error;
    }
    // 唤醒部分等待者
    w_lock.unlock();
    tickle();
    // std::cout << "\n pool state: free:" << m_freeConnInfos.size();
    return 0;
}

int MySQLPoolManager::expand() {
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
        int increaseNum = MYSQL_CONNECT_CREASE_SPEED;
        int temp = m_maxConnector - m_connectorCount;
        increaseNum =  (increaseNum < temp) ? increaseNum : temp;

        // 扩容
        int rt = 0;
        int max_idx = 0;
        for(int i = m_connectorCount; i < m_connectorCount + increaseNum && !rt; i++) {
            rt = m_connectors[i]->connect(  m_connectorBaseInfo.host, 
                                            m_connectorBaseInfo.user, 
                                            m_connectorBaseInfo.passwd, 
                                            m_connectorBaseInfo.db, 
                                            m_connectorBaseInfo.port, 
                                            m_connectorBaseInfo.clientflag);
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
            m_state = MySQLPoolManager::READY;
        }
        return 0;
    }
}

}