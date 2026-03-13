#include "basic/log.h"
#include "basic/macro.h"
#include "basic/thread.h"
#include "coroutine/coro20/fiber.h"
#include "coroutine/coro20/scheduler.h"
#include <sys/param.h>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <shared_mutex>

namespace m_sylar
{

static Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");
static thread_local Scheduler* tl_Scheduler;        // 当前线程调度器指针

Scheduler::Scheduler (const std::string& name, size_t thread_num )
    : m_name(name), m_threads_count(thread_num)
    , m_autoStop(false), m_Stop(false)
    , m_activeThreadCount(thread_num)
{
}

Scheduler::~Scheduler()
{
    stop();     // 强制停止
}
    
void Scheduler::start()
{   
    M_SYLAR_ASSERT2(!getTaskCount() && !m_threads.size(), "m_tasks is not null or threadsCount is not 0");
    std::unique_lock<std::shared_mutex> w_lock(m_mutex);
    for(int i = 0; i < m_threads_count; i++)
    {
        Thread::ptr new_thread(new m_sylar::Thread(std::bind(&Scheduler::ThreadInit, this),
                                m_name + "_" + std::to_string(i)));
        m_threads.push_back(new_thread);
    }
    M_SYLAR_ASSERT(m_threads.size() == m_threads_count  );
}

void Scheduler::setThis()
{
    tl_Scheduler = this;
}

Scheduler* Scheduler::GetThis()
{
    return tl_Scheduler;
}

void Scheduler::ThreadInit()
{
    // Task t = run();
    // while(!t.getHandle().done())
    // {
    //     M_SYLAR_LOG_WARN(g_logger) << "main fiber expectly suspend";
    //     t.getHandle().resume();
    // }
    run();
}

void Scheduler::run()
{   // 调度器有任务运行时
    setThis();
    std::list<TaskCoro20> work_list;
    while(true)
    {
        // 取任务
        {
            std::unique_lock<std::shared_mutex> r_lock(m_mutex);
            // auto it = m_tasks.begin();
            int task_len = MIN(10, m_tasks.size());
            auto begin = m_tasks.begin();
            auto end = std::next(begin, task_len);;
            // if(it != m_tasks.end())
            // {
            //     curr_task = std::move(*it);
            //     m_tasks.erase(it);
            // }
            work_list.splice(work_list.end(), m_tasks, begin, end);
            if(m_tasks.size() && m_idleThreadCount)
            {
                tickle();
            }
        }
        // 执行任务
        TaskCoro20 curr_task;
        while(work_list.size())
        {
            curr_task = std::move(*work_list.begin());
            work_list.pop_front();
            if(curr_task.isLegal() && !curr_task.isFinished())
            {
                curr_task.start();
            }
        }

        M_SYLAR_ASSERT2(!work_list.size(), "[coro scheduler]: work_list is unempty");

        // 下一步，结束/idle-下一个任务
        if(m_Stop || (getTaskCount() == 0 && m_autoStop))
        {
            // 强停止或自动无任务停止
            break;
        }
        if(getTaskCount())
        {
            continue;
        }
        else
        {
            // TaskCoro20 idle_task(std::bind(&Scheduler::idle, this));
            // std::cout << "task resume\n";
            // idle_task.resume();
            idle();
        }
    }
    // 线程池终止 执行清理
    std::unique_lock<std::shared_mutex> w_lock(m_mutex);
    m_threads_count--;  // 减少线程池内部线程数量。
    return; 
}

void Scheduler::idle()
{   // 调度器空闲时
    m_idleThreadCount++;
    m_activeThreadCount--;
    M_SYLAR_LOG_DEBUG(g_logger) << "thread is idle";
    sleep(1);
    m_idleThreadCount--;
    m_activeThreadCount++;
}

void Scheduler::tickle()
{
    // M_SYLAR_LOG_DEBUG(g_logger) << "tickle othres";
}

void Scheduler::autoStop()
{   // 注意，autoStop会截断正在运行的协程
    m_autoStop = true;
    while(getTaskCount())
    {
        usleep(10000);
    }
    while(m_threads_count)
    {
        usleep(10000);
        tickle();
    }
}

void Scheduler::stop()
{
    m_Stop = true;
    while(m_threads_count)
    {
        usleep(10000);
        tickle();
    }
}

bool Scheduler::isStopping()
{
    return m_Stop || m_autoStop;
}




}