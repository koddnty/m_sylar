#include "factory.h"


namespace m_sylar
{
static Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");
MySQLPoolManager::ptr MySQLPoolInstance = nullptr;

DBPool<MySQLConn, MySQLResp>::ptr DB::createMysqlPool(int min_conn, int max_conn, bool is_public) {
    MySQLPoolManager::ptr pool = std::make_shared<MySQLPoolManager> (min_conn, max_conn);
    if(is_public) {
        MySQLPoolInstance = pool;
    }
    return pool;
}

MySQLPoolManager::ptr DB::Mysql::getInstance() {
    if(MySQLPoolInstance) {
        return MySQLPoolInstance;
    }
    createMysqlPool(5, 10, true);
    return MySQLPoolInstance;
}
}