#include <iostream>
#include "DBPool/mysql.h"
#include "coroutine/corobase.h"

m_sylar::MySQLPoolManager* mysql_pool_mgr = nullptr; 
std::atomic<int> count = 0;


m_sylar::Task<void, m_sylar::TaskBeginExecuter> testNext () {
    std::string sql = "select * from learn";
    m_sylar::MySQLResp::ptr resp = co_await mysql_pool_mgr->executeQuery(sql);
    if(resp) {
        auto row = resp->nextRow();
        while(row) {
            auto col = row.nextValue();
            while(col) {
                std::cout << col.get() << " ";
                col = row.nextValue();
            }
            row = resp->nextRow();
        }
        std::cout << std::endl;
    }
    else {
        std::cout << "failed to execute query" << std::endl;
    }
}


m_sylar::Task<void, m_sylar::TaskBeginExecuter> testMap() {
    std::string sql = "SELECT SLEEP(5)";
    sleep(5);
    m_sylar::MySQLResp::ptr resp = co_await mysql_pool_mgr->executeQuery(sql);
    resp->formatDate();
    // std::cout << (*resp)["telephone_num"][4] << std::endl;
    std::cout << "|" << std::flush;
}

int main(void) {
    m_sylar::MySQLPoolManager dbPool(10, 20);
    //dbPool.init("<地址>", "<用户名>", "<数据库密码>", "<数据库名称>", <端口>, 0))
    if(-1 == dbPool.init("localhost", "koddnty", "73256", "KoddntyDB", 3306, 0)) {
        std::cout << "failed to init dbPool" << std::endl;
    }
    else {
        std::cout << "init dpPool successed" << std::endl;
    }
    mysql_pool_mgr = &dbPool;

    m_sylar::IOManager iom("test_dbPool", 1);


    for(int i = 0; i < 30; i++) {
        // iom.schedule(m_sylar::TaskCoro20::create_coro(testNext));
        iom.schedule(m_sylar::TaskCoro20::create_coro(testMap));
    }

    sleep(30);       // 等待执行到一半的协程任务。
    iom.autoStop();
    dbPool.close();
    return 0;
}