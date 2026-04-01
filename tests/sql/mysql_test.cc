#include <iostream>
#include "DBPool/mysql.h"
#include "coroutine/corobase.h"


m_sylar::Task<void, m_sylar::TaskBeginExecuter> sqlTask(m_sylar::MySQLDB* mysql) {

    std::string sql = "select * from learn";
    m_sylar::MySQLResp::ptr task = co_await mysql->executeQuery(sql);

    for(int i = 0; i < 4; i++)
    {
        for(auto row = task->nextRow(); row; row = task->nextRow()) {
            for(auto value = row.nextValue(); value; value = row.nextValue()) {
                std::cout << value.get() << " ";
            }
            std::cout << std::endl;
        }
        std::cout << "------------------" << std::endl;
        task->resetRow();
    }

    co_return;
}


int main(void) {

    m_sylar::MySQLDB mysql;
    try{
        mysql.connect("localhost", "koddnty", "73256", "KoddntyDB", 3306, 0);
    } 
    catch(std::exception& e) {
        std::cout << e.what();
    }

    m_sylar::IOManager iom("test_mysql");
    iom.schedule(m_sylar::TaskCoro20::create_coro(std::bind(sqlTask, &mysql)));
        std::cout << "========================" << std::endl;
    sleep(1);
    iom.schedule(m_sylar::TaskCoro20::create_coro(std::bind(sqlTask, &mysql)));
        std::cout << "========================" << std::endl;
    sleep(1);
    iom.schedule(m_sylar::TaskCoro20::create_coro(std::bind(sqlTask, &mysql)));
        std::cout << "========================" << std::endl;

    sleep(3);
    iom.autoStop();
    return 0;
}