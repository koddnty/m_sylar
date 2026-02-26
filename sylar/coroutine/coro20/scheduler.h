#pragma once
#include "basic/log.h"
#include "basic/macro.h"
#include "basic/thread.h"
#include "coroutine/coro20/fiber.h"
#include <shared_mutex>
#include <coroutine>
#include <cstddef>
#include <functional>
#include <list>
#include <memory>
#include <mutex>


namespace m_sylar
{
// static Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

class Scheduler
{
public:
    using ptr = std::shared_ptr<Scheduler>;

    Scheduler (const std::string& name, size_t thread_num = 1);
    virtual ~Scheduler ();

    void start();
    void setThis();

    std::string getName () {return m_name;}
    static Scheduler* GetThis();                              // 获取当前线程
    int getTaskCount() const {return m_tasks.size(); }


    template<typename Func_or_Handle>
    void schedule(Func_or_Handle cb)
    {
        {
            std::unique_lock<std::shared_mutex> w_lock(m_mutex);
            scheduleNoLock(std::move(cb));
        }
        tickle();
    }

    template<typename InputOperator>
    void schedule(InputOperator begin, InputOperator end)
    {
        {
            std::unique_lock<std::shared_mutex> w_lock(m_mutex);
            for(;begin != end; begin++)
            {
                scheduleNoLock(std::move(*begin));
            }
        }
        tickle();
    }

private:
    template<typename Func_or_Handle>           // 参数是普通函数或者协程函数
    void scheduleNoLock(Func_or_Handle&& f)
    {
        TaskCoro20 task(f);
        if(task.isFinished()) {return;}
        if(m_autoStop)
        {
            return;
        }
        // TaskCoro20 t(task);
        M_SYLAR_ASSERT2(!task.isFinished(), "[scheduleCoro20] illegal Task");
        // m_tasks.push_back(task);
        m_tasks.push_back(std::move(task));
    }

public:
    void ThreadInit();
    void run();
    virtual void idle();
    virtual void tickle();
    virtual void autoStop();
    virtual void stop();
    virtual bool isStopping();

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