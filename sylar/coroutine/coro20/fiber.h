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
class TaskCoro20
{
public:
    using HandlePtr = std::shared_ptr<std::coroutine_handle<>>;
    using ptr = std::shared_ptr<TaskCoro20>;

    TaskCoro20() {m_status = UNSET;}
    TaskCoro20(std::function<Task<int>()> cb);
    TaskCoro20(std::function<void()> cb);
    TaskCoro20(HandlePtr handle);
    ~TaskCoro20();         

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




// template<typename ResultType, typename Executer>
class Fiberf
{
public: 
    using ptr = std::shared_ptr<Fiberf>;
    enum class Type
    {
        UNKNOWN = 0,
        CORO = 1,
        FUNC = 2,
    };

public: 
    Fiberf(std::function<void()> && task)
        : m_task(std::move(task)){ }

    Fiberf(std::coroutine_handle<> handle)
        : m_handle(std::move(handle)) {}


private:    
    Type m_type;
    std::function<void()> m_task;
    std::coroutine_handle<> m_handle;
};


class suspendExecuter : public AbstractExecuter
{   // 挂起当前协程
public:
    virtual void execute(std::function<void()> &&func) override
    { 
        // 注册协程回调任务
        Fiberf task(std::move(func));
        
    }
};



}

