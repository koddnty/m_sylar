//
// Created by koddnty on 2026/7/24.
//
#include <DBPool/mysql.hpp>
#include "DBPool/factory.h"
#include "coroutine/corobase.h"

using namespace m_sylar;
static Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

static Task<void, TaskBeginExecuter> testStmtText(MySQLPoolManager::ptr pool) {
    auto connection = pool->borrowOneConn();
    const std::string sql = "select name from learn where telephone_num = ?";
    MySQLStmt<STMT_Text<255>> stmt (connection);
    co_await stmt.co_execute<std::string>(sql, "99999999999");
    co_await stmt.co_storeAll();
    co_await stmt.co_fetchAll();
    // co_await stmt.co_fetchNext();

    const auto text = std::get<0>(stmt.getResult().getAll()[0]);
    const std::string resp = TextToString(text);
    M_SYLAR_LOG_INFO(g_logger) << "stmt seek sql finished, result: "
            << resp;
    co_await stmt.co_close();
    co_return;
}



static Task<void, TaskBeginExecuter> testStmtInt(MySQLPoolManager::ptr pool) {
    auto connection = pool->borrowOneConn();
    const std::string sql = "select name from learn where gender = ?";
    MySQLStmt<STMT_Text<255>> stmt (connection);
    co_await stmt.co_execute<std::string>(sql, "0");
    co_await stmt.co_storeAll();
    co_await stmt.co_fetchAll();
    // co_await stmt.co_fetchNext();

    M_SYLAR_LOG_INFO(g_logger) << "stmt seek sql finished, result: ";
    for (auto it : stmt.getResult().getAll()) {
        std::cout << std::get<0>(it).toString() << std::endl;
    }
    co_await stmt.co_close();
    co_return;
}


int main() {
    // 配置导入
    std::string config_path = "/home/koddnty/user/projects/sylar/m_sylar/m_sylar/conf/basic.json";
    std::cout << "[LoggerManager init] config path: " << config_path << std::endl;
    m_sylar::ConfigManager::LoadJson(config_path, 0);

    // 数据库连接池的创建与连接
    MySQLPoolManager dbPool(10, 15);
    MySQLPoolManager::ptr mysql_pool = m_sylar::DB::createMysqlPool(10, 15);

    if(-1 == mysql_pool->init("localhost", "koddnty", "73256", "KoddntyDB", 3306, 0)) {
        std::cout << "failed to init dbPool" << std::endl;
        return -1;
    }
    std::cout << "init dpPool succeed" << std::endl;

    IOManager iom("mysql_stmt_test", 1);

    iom.schedule(TaskCoro20::create_coro(std::bind(&testStmtText, mysql_pool)));
    iom.schedule(TaskCoro20::create_coro(std::bind(&testStmtInt, mysql_pool)));


    // 退出
    sleep(30);
    iom.autoStop();


}