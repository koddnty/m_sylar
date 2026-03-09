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
    // std::cout << "a coroutine task started" << std::endl;
    count++;
    co_return;
}


void test_coroutine_task()
{
    m_sylar::Scheduler scheduler("test_coroutine", 1);
    scheduler.start();
    std::cout << "schedule task ..." << std::endl;
    // scheduler.schedule(task);
    // scheduler.schedule(static_cast<std::function<m_sylar::Task<void, m_sylar::TaskBeginExecuter>()>>(task));
    // // scheduler.schedule(func_task);
    // scheduler.schedule(static_cast<std::function<void()>>(func_task));

    for(int i = 0; i < 1000; i++)
    {
        // scheduler.schedule(func_task);
        scheduler.schedule(static_cast<std::function<m_sylar::Task<void, m_sylar::TaskBeginExecuter>()>>(task));
    }

    std::cout << "schedule task finished" << std::endl;
    scheduler.getTaskCount();
    scheduler.autoStop();
    std::cout << count << std::endl;
}

int main(void)
{
    test_coroutine_task();
    return 0;
}