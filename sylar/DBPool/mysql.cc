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
    m_sylar::IOManager* iom = m_sylar::IOManager::getInstance();
    // 回调事件注册
    iom->addEvent(m_fd, m_event, [this](){  
        resume();
    });

}

void MysqlAwaiter::before_resume() {
}



// 响应封装
MySQLResp::MySQLResp(MYSQL* mysql){
    m_respBody = mysql_store_result(mysql);
    m_colCount = mysql_field_count(mysql);

}

MySQLResp::~MySQLResp(){
    mysql_free_result(m_respBody);
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
        throw std::runtime_error("MySQLResp error, out of index");
    }
    return std::string(m_data, m_length);
}

char* MySQLResp::Value::get(size_t& length){
    if(!m_isValid){
        throw std::runtime_error("MySQLResp error, out of index");
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
    auto row = mysql_fetch_row(m_respBody);
    auto len = mysql_fetch_lengths(m_respBody);
    return {row, m_colCount, len};
}







// MYSQL 封装
MySQLDB::MySQLDB(){
    m_mysql = mysql_init(nullptr);
    if(m_mysql == nullptr) {
        M_SYLAR_LOG_ERROR(g_logger) << "failed to init a mysql instence";
        throw std::runtime_error("failed to init a mysql instence");
    }
    m_state == State::INIT;
}

MySQLDB::~MySQLDB(){
    if(m_state != State::INIT && m_mysql) {
        std::cout << "mysql 析构， fd = " << mysql_get_socket(m_mysql) << std::endl;
        IOManager::getInstance()->delEvent( mysql_get_socket(m_mysql), (FdContext::Event)(FdContext::Event::READ | FdContext::Event::WRITE));
        IOManager::getInstance()->closeWithNoClose( mysql_get_socket(m_mysql));
        mysql_close(m_mysql);
    }
}

MySQLDB& MySQLDB::connect(const std::string& host,
                        const std::string& user,
                        const std::string& passwd,
                        const std::string& db,
                        unsigned int port,
                        unsigned long clientflag) {

    MYSQL* rt = mysql_real_connect(m_mysql, host.c_str(), user.c_str(), passwd.c_str(), 
                                    db.c_str(), port, NULL, clientflag);
    if(rt == nullptr) {
        M_SYLAR_LOG_ERROR(g_logger) << "connect failed, error: " << mysql_error(m_mysql);
        throw std::runtime_error(mysql_error(m_mysql));
    }

    // 状态同步
    if (mysql_options(m_mysql, MYSQL_OPT_NONBLOCK, 0)) {
        std::cout << "1mysql_optins failed : " << mysql_error(m_mysql) << std::endl;
    }
    // m_mysql = rt;
    m_state = State::READY;
    return *this;
}


Task<MySQLResp::ptr> MySQLDB::executeQuery(const std::string& query){
    int err = 0;
    if (mysql_options(m_mysql, MYSQL_OPT_NONBLOCK, 0)) {
        std::cout << "mysql_optins failed : " << mysql_error(m_mysql) << std::endl;
    }
    int status = mysql_real_query_start(&err, m_mysql, query.c_str(), query.length());
    
    if(err) {
        std::cout << std::string("mysql_real_query_start failed, error : ") + mysql_error(m_mysql) << std::endl;
        std::string errinfo = std::string("mysql_real_query_start failed, error : ") + mysql_error(m_mysql);
        throw std::runtime_error(errinfo.c_str());
    }

    if(status == 0) {
        co_return std::make_shared<MySQLResp>(m_mysql);
    }



    while(status){
        co_await MysqlAwaiter(m_mysql, status);
        status = mysql_real_query_cont(&err, m_mysql, status);
        if(err) {
            std::string errinfo = std::string("mysql_real_query_start failed, error : ") + mysql_error(m_mysql);
            throw std::runtime_error(errinfo.c_str());
        }
    }

    co_return std::make_shared<MySQLResp>(m_mysql);
}

}