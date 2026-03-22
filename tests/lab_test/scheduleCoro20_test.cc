#include "coroutine/corobase.h"
#include "basic/log.h"
#include <atomic>
#include <ostream>


std::atomic<int> count = 0;

void func()
{
    count++;
}

void test_func_task()
{
    m_sylar::Scheduler scheduler("test_coro_schedule", 12) ;
    scheduler.start();

    for(int i = 0; i < 10000; i++)
    {
        scheduler.schedule(func);
    }
    std::cout << scheduler.getTaskCount() << std::endl;
    sleep(10);
    // sleep(1000);
    std::cout << count << std::endl;
    std::cout << scheduler.getTaskCount() << std::endl;
    scheduler.autoStop();
}



void func_task()
{
    // std::cout << " func task running..." << std::endl;
    count++;
    return;
}

m_sylar::Task<void, m_sylar::TaskBeginExecuter> task()
{
    m_sylar::IOManager::getInstance()->schedule(m_sylar::TaskCoro20::create_coro(task));
    count++;
    co_return;
}


void test_coroutine_task()
{
    // m_sylar::Scheduler scheduler("test_coroutine", 1);
    m_sylar::IOManager iom("test_coroutine", 4);
    std::cout << "schedule task ..." << std::endl;

    int total = 100000;
    for(int i = 0; i < total; i++)
    {
        iom.schedule(m_sylar::TaskCoro20::create_coro(task));
    }

    std::cout << "schedule task finished" << std::endl;
    iom.getTaskCount();
    // iom.autoStop();
    int time = 120;
    sleep(time);
    std::cout << "have runed " << count << " in " << time << " sec" << std::endl;
    iom.stop();
}

int main(void)
{
    test_coroutine_task();
    return 0;
}