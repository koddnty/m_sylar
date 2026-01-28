#include "basic/log.h"
#include "basic/macro.h"
#include "basic/thread.h"
#include "coroutine/coro20/fiber.h"
#include "coroutine/coro20/scheduler.h"
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <shared_mutex>

namespace m_sylar
{

static thread_local SchedulerCoro20* tl_schedulerCoro20;        // 当前线程调度器指针

SchedulerCoro20::SchedulerCoro20 (const std::string& name, size_t thread_num )
    : m_name(name), m_threads_count(thread_num)
    , m_autoStop(false), m_Stop(false)
    , m_activeThreadCount(thread_num)
{
}

SchedulerCoro20::~SchedulerCoro20()
{
    stop();     // 强制停止
}
    
void SchedulerCoro20::start()
{   
    M_SYLAR_ASSERT2(!m_tasks.size() && !m_threads.size(), "m_tasks is not null or threadsCount is not 0");
    std::unique_lock<std::shared_mutex> w_lock(m_mutex);
    for(int i = 0; i < m_threads_count; i++)
    {
        Thread::ptr new_thread(new m_sylar::Thread(std::bind(&SchedulerCoro20::run, this),
                                m_name + "_" + std::to_string(i)));
        m_threads.push_back(new_thread);
    }
    M_SYLAR_ASSERT(m_threads.size() == m_threads_count  );
}

void SchedulerCoro20::setThis()
{
    tl_schedulerCoro20 = this;
}

SchedulerCoro20* SchedulerCoro20::GetThis()
{
    return tl_schedulerCoro20;
}

void SchedulerCoro20::run()
{
    setThis();
    while(true)
    {
        
        TaskCoro20 curr_task;
        // 取任务
        {
            std::unique_lock<std::shared_mutex> r_lock(m_mutex);
            auto it = m_tasks.begin();
            if(it != m_tasks.end())
            {
                curr_task = *it;
                m_tasks.erase(it);
            }
        }
        // 执行任务
        if(curr_task.isLegal())
        {
            curr_task.resume();
        }
        // 下一步，结束-idle-下一个任务
        if(m_Stop || (m_tasks.size() == 0 && m_autoStop))
        {
            break;
        }
        if(m_tasks.size())
        {
            continue;
        }
        else
        {
            TaskCoro20 idle_task(std::bind(&SchedulerCoro20::idle, this));
            // std::cout << "task resume\n";
            idle_task.resume();
        }
    }
    // 线程池终止 执行清理
    std::unique_lock<std::shared_mutex> w_lock(m_mutex);
    m_threads_count--;  // 减少线程池内部线程数量。
}

void SchedulerCoro20::idle()
{
    m_idleThreadCount++;
    m_activeThreadCount--;
    M_SYLAR_LOG_DEBUG(g_logger) << "thread is idle";
    sleep(1);
    m_idleThreadCount--;
    m_activeThreadCount++;
}

void SchedulerCoro20::tickle()
{
    // M_SYLAR_LOG_DEBUG(g_logger) << "tickle othres";
}

void SchedulerCoro20::autoStop()
{
    m_autoStop = true;
    while(m_tasks.size())
    {
        usleep(10000);
    }
    while(m_threads_count)
    {
        usleep(10000);
        tickle();
    }
    
}

void SchedulerCoro20::stop()
{
    m_Stop = true;
    while(m_threads_count)
    {
        usleep(10000);
        tickle();
    }
}

bool SchedulerCoro20::isStopping()
{
    return m_Stop || m_autoStop;
}




}