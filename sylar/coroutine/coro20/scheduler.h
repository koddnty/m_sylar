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
#include "taskPool.h"


namespace m_sylar
{


class Scheduler
{
public:
    using ptr = std::shared_ptr<Scheduler>;

    Scheduler (const std::string& name, size_t thread_num = 1, size_t taskLoopNum = 0);
    virtual ~Scheduler ();

    void start();
    void setThis();

    std::string getName () {return m_name;}
    static Scheduler* GetThis();                              // 获取当前线程
    int getTaskCount() {
        return m_task_pool.getTaskCount(); 
    }


    template<typename Func_or_Handle>
    void schedule(Func_or_Handle cb)
    {
        scheduleNoLock(std::move(cb));
        tickle();
    }

    template<typename InputOperator>
    void schedule(InputOperator begin, InputOperator end)
    {
        for(;begin != end; begin++)
        {
            scheduleNoLock(std::move(*begin));
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
        M_SYLAR_ASSERT2(!task.isFinished(), "[scheduleCoro20] illegal Task");
        m_task_pool.taskAlloc(std::move(task));
    }

    void scheduleNoLock(TaskCoro20&& task)
    {
        if(task.isFinished()) {return;}
        if(m_autoStop)
        {
            return;
        }
        M_SYLAR_ASSERT2(!task.isFinished(), "[scheduleCoro20] illegal Task");
        m_task_pool.taskAlloc(std::move(task));
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
    // std::list<TaskCoro20> m_tasks;                                  // 任务队列
    // std::map<std::pair<int, int>, Thread::ptr> m_threads;           //{{线程序号，线程号}， 线程指针}
    std::vector<Thread::ptr> m_threads;
    int m_threads_count;                                            // 线程个数
    bool m_autoStop;                                                // 软停止（执行完毕所有任务
    bool m_Stop;                                                    // 直接停止
    std::atomic<size_t> m_activeThreadCount = {0};                  // 活跃线程数
    std::atomic<size_t> m_idleThreadCount = {0};                    // 空闲线程数
    std::shared_mutex m_mutex;

    TaskPool m_task_pool;           // 任务池

};
}