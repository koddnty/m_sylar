#include "coroutine/corobase.h"
#include "basic/log.h"
#include <atomic>
#include <ostream>


std::atomic<int> count = 0;

void func()
{
    // std::cout << "run Func" << std::endl;
    count++;
    // std::cout << "1";
}

int main(void)
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
    return 0;
}