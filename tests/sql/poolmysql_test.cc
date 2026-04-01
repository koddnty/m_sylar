#include <iostream>
#include "DBPool/mysql.h"
#include "coroutine/corobase.h"

m_sylar::MySQLPoolManager* mysql_pool_mgr = nullptr; 
std::atomic<int> count = 0;


m_sylar::Task<void, m_sylar::TaskBeginExecuter> task () {
    std::string sql = "select * from learn";
    
    m_sylar::MySQLResp::ptr resp = co_await mysql_pool_mgr->executeQuery(sql);
    if(resp) {
        auto row = resp->nextRow();
        while(row) {
            auto col = row.nextValue();
            while(col) {
                std::cout << "-";
                // std::cout << col.get() << " ";
                col = row.nextValue();
            }
            // std::cout << std::endl;
            row = resp->nextRow();
        }
        std::cout << "|" << count++ << std::flush;
    }
    else {
        std::cout << "failed to execute query" << std::endl;
    }
}


int main(void) {
    m_sylar::MySQLPoolManager dbPool(10, 20);
    if(-1 == dbPool.init("localhost", "koddnty", "73256", "KoddntyDB", 3306, 0)) {
        std::cout << "failed to init dbPool" << std::endl;
    }
    else {
        std::cout << "init dpPool successed" << std::endl;
    }
    mysql_pool_mgr = &dbPool;

    m_sylar::IOManager iom("test_dbPool", 1);


    for(int i = 0; i < 3000; i++) {
        iom.schedule(m_sylar::TaskCoro20::create_coro(task));
    }

    sleep(10);
    iom.autoStop();
    sleep(5);
    dbPool.close();
    return 0;
}