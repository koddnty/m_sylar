#pragma once
#include "basic/fiber.h"
#include "basic/ioManager.h"
#include "basic/log.h"
#include "basic/macro.h"
#include "basic/thread.h"
#include <coroutine>
#include <cstddef>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include "fiber.h"


namespace m_sylar
{
static Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

class SchedulerCoro20
{
public:
    using ptr = std::shared_ptr<SchedulerCoro20>;

    SchedulerCoro20 (const std::string& name, size_t thread_num = 1);
    virtual ~SchedulerCoro20 ();

    void start();
    void setThis();

    std::string getName () {return m_name;}
    static SchedulerCoro20* GetThis();                              // 获取当前线程
    int getTaskCount() const {return m_tasks.size(); }


    template<typename Func_or_Handle>
    void schedule(Func_or_Handle cb)
    {
        std::unique_lock<std::shared_mutex> w_lock(m_mutex);
        if(!cb)
        {
            M_SYLAR_LOG_ERROR(g_logger) << "[scheduleCoro20]cb is nullptr";
            return;
        }
        scheduleNoLock(cb);
    }

    template<typename InputOperator>
    void schedule(InputOperator begin, InputOperator end)
    {
        std::unique_lock<std::shared_mutex> w_lock(m_mutex);
        for(;begin != end; begin++)
        {
            scheduleNoLock(*begin);
        }
    }

private:
    template<typename Func_or_Handle>
    void scheduleNoLock(Func_or_Handle task)
    {
        if(m_autoStop)
        {
            return;
        }
        TaskCoro20 t(task);
        M_SYLAR_ASSERT2(t.isLegal(), "[scheduleCoro20] illegal Task");
        m_tasks.push_back(t);
    }

public:
    void run();
    void idle();
    virtual void tickle();
    void autoStop();
    void stop();
    bool isStopping();

protected:
    std::string m_name;                                             // schedule名称
    std::list<TaskCoro20> m_tasks;                                  // 任务队列
    // std::map<std::pair<int, int>, Thread::ptr> m_threads;           //{{线程序号，线程号}， 线程指针}
    std::vector<Thread::ptr> m_threads;
    int m_threads_count;                                            // 线程个数
    bool m_autoStop;                                                // 软停止（执行完毕所有任务
    bool m_Stop;                                                    // 直接停止
    std::atomic<size_t> m_activeThreadCount = {0};                  // 活跃线程数
    std::atomic<size_t> m_idleThreadCount = {0};                    // 空闲线程数
    std::shared_mutex m_mutex;

};
}