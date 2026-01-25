#include "lab/schedule.h"
#include "basic/log.h"


void func()
{
    std::cout << "run Func" << std::endl;
}

int main(void)
{
    m_sylar::SchedulerCoro20 scheduler("test_coro_schedule", 1) ;
    scheduler.start();

    scheduler.schedule(func);
    // sleep(10);
    scheduler.autoStop();
    return 0;
}