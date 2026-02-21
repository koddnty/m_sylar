#pragma once
#include <cstdlib>
#include <functional>
#include <coroutine>
#include <memory>
#include <coroutine>
#include "basic/log.h"
#include "basic/macro.h"
#include "coroutine/coro20/task.hpp"


namespace m_sylar
{


// template<typename int>
class TaskCoro20_old
{
public:
    using HandlePtr = std::shared_ptr<std::coroutine_handle<>>;
    using ptr = std::shared_ptr<TaskCoro20_old>;

    TaskCoro20_old() {m_status = UNSET;}
    TaskCoro20_old(std::function<Task<int>()> cb);
    TaskCoro20_old(std::function<void()> cb);
    TaskCoro20_old(HandlePtr handle);
    ~TaskCoro20_old();         

    void setTask(std::function<Task<int>()> cb);
    void setTask(std::function<void()> cb);
    void setHandle(HandlePtr handle);       


          
    void reset();
    void resume();
    
    bool isLegal()
    {   // 如果任务是有效任务返回true
        switch(m_status)
        {
            case UNSET:
                return false;
                break;
            case INIT:
                return !!m_task;
                break;
            case SUSPEND:
                return !!m_handler;
                break;
            case EXEC:
                return !!m_handler;
                break;
            case TERM:
                return false;
                break;
            default:
                return false;
        }
    }

    int getResult();

private:
    enum Status
    {
        UNSET = -1,         // 默认构造状态
        INIT = 0,           // 未进行调用过，无handle
        SUSPEND = 1,        // handle已经注册
        EXEC = 2,           // 运行中
        TERM = 3            // 结束
    };

    // Task<> Fiberunc(std::function<void()> cb);
    // void task()
private:
    std::function<Task<int>()> m_task;
    Status m_status;            // 协程状态
    HandlePtr m_handler;        // 协程句柄 
};


class TaskBeginExecuter : public AbstractExecuter
{   // 此调度器在任务创建时挂起任务， 执行期间不挂起任务
public:
    virtual void execute(std::function<void()> &&func)
    {
        func();
    }

    virtual void initialExecute(std::function<void()> &&func)
    {
    }
};


class TaskCoro20
{
public:
    using ptr = std::shared_ptr<TaskCoro20>;
    
    TaskCoro20(std::function<Task<void, TaskBeginExecuter>()> task)
        : m_task(task()){}

    TaskCoro20(std::function<void()> task)
        : m_func_task(task), m_task(std::bind(&TaskCoro20::runner, this)()){ }

    TaskCoro20(TaskCoro20&& other)
        : m_task(std::move(other.m_task)) { }
    
    TaskCoro20() {}


    void start()
    {
        if(m_task.getHandle().done())
        {
            std::cout << "[coroutine]resume a finished handle" << std::endl;
        }
        m_task.getHandle().resume();
    }

    bool isFinished()
    {
        return m_task.getHandle().done();
    }

    void finally(std::function<void(Result<void>)>&& func)
    {
        m_task.finally(std::move(func));
    }

private:
  Task<void, TaskBeginExecuter> runner()
  {
        m_func_task();
        co_return;
  };

private:
    std::function<void()> m_func_task = nullptr;
    Task<void, TaskBeginExecuter> m_task;
};


}

