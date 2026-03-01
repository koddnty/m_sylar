// #include "basic/ioManager.h"
#include "coroutine/corobase.h"
#include <iostream>

int count = 0;

m_sylar::Task<void, m_sylar::TaskBeginExecuter> func()
{
    count++;
    co_return;
    // std::cout << 1;
}

m_sylar::Task<int, m_sylar::TaskBeginExecuter> task()
{
    std::cout << "|" << std::endl;
    count++;
    co_return 1;
}


int main(void)
{
    // m_sylar::Scheduler iom("test_coroutine", 1);
    // iom.start();

    m_sylar::IOManager iom("test_scheduler", 1);

    int total = 10000;
    // int total = 10000;
    auto schedule_start = std::chrono::high_resolution_clock::now();
    for(int i = 0; i < total; i++)
    {
        iom.schedule(static_cast<std::function<m_sylar::Task<int, m_sylar::TaskBeginExecuter>()>>(task));
        // iom.schedule(static_cast<std::function<m_sylar::Task<void, m_sylar::TaskBeginExecuter>()>>(func));
        // iom.schedule( func);
    }
    auto schedule_end = std::chrono::high_resolution_clock::now();
    // iom.schedule(func);
    // sleep(sec);
    // sleep(1);
    // std::cout << "task count" << iom.getTaskCount() << std::endl;
    // sleep(2);
    // std::cout << "task count" << iom.getTaskCount() << std::endl;
    // sleep(4);
    // std::cout << "task count" << iom.getTaskCount() << std::endl;


    int task_begin = total - iom.getTaskCount();    // 开始时运行过的任务数量
    auto solve_begin = std::chrono::high_resolution_clock::now();
    std::cout << "count: " << count << std::endl;
    std::cout << "task count: " << task_begin << std::endl;
    std::cout << "要sleep了啊" << std::endl;
    // sleep(2);
    std::cout << "after sleep 2s" << std::endl;
    iom.autoStop();
    auto solve_end = std::chrono::high_resolution_clock::now();

    std::cout << "schedule time : " << ( schedule_end - schedule_start).count() << std::endl;
    std::cout << count - task_begin << " task solved time : " << (solve_end - solve_begin).count() << std::endl;

    return 0;
}