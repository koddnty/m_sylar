#include <iostream>
#include "DBPool/redis.h"
#include "coroutine/corobase.h"
#include "DBPool/factory.h"


m_sylar::Task<void, m_sylar::TaskBeginExecuter> test (m_sylar::RedisPoolManager* redisPool) {
    std::string sql = "QUIT";
    m_sylar::RedisResp::ptr resp = co_await redisPool->executeQuery(sql);
    if(resp) {
        std::cout << resp->asString() << std::endl;
    }
    else {
        std::cout << "failed to execute query" << std::endl;
    }
    co_return;
}


int main(void) {
    m_sylar::RedisPoolManager dbPool(10, 15);
    //dbPool.init("<地址>", "<用户名>", "<数据库密码>", "<数据库名称>", <端口>, 0))
    int rt =dbPool.init("localhost", 6379);

    if(-1 == rt) {
        std::cout << "failed to init dbPool" << std::endl;
    }
    else {
        std::cout << "init dbPool successed" << std::endl;
    }

    m_sylar::IOManager iom("test_dbPool", 1);


    for(int i = 0; i < 30; i++) {
        // iom.schedule(m_sylar::TaskCoro20::create_coro(testNext));
        iom.schedule(m_sylar::TaskCoro20::create_coro(std::bind(test, &dbPool)));
    }

    sleep(30);       // 等待执行到一半的协程任务。
    iom.autoStop();
    dbPool.close();
}