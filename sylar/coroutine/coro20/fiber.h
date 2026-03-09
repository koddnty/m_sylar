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


class TaskBeginExecuter : public AbstractExecuter
{   // 此调度器在任务创建时挂起任务， 执行期间不挂起任务
public:
    TaskBeginExecuter()
    {

    }

    ~TaskBeginExecuter()
    {
    }
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
    
    template<typename _Result>
    explicit TaskCoro20(std::function<Task<_Result, TaskBeginExecuter>()> task)     // 协程任务
        :  m_coro_task(task), m_task(task()){
        m_type = CORO;
    }    

    explicit TaskCoro20(std::function<void()> task)      // 函数任务
        : m_func_task(task){ 
        m_type = FUNC;
    }

    TaskCoro20() {
        m_is_inited = false;
    }

    // 拷贝构造
    TaskCoro20& operator=(const TaskCoro20& other) = delete;
    // {
    //     m_type = other.m_type;
    //     if(m_type == CORO)
    //     {
    //         m_coro_task = other.m_coro_task;
    //         m_task = m_coro_task();
    //         m_is_inited = other.m_is_inited;
    //         m_finished = false;
    //     }
    //     else if(m_type == FUNC)
    //     {
    //         m_func_task = other.m_func_task;
    //         m_is_inited = other.m_is_inited;
    //         m_finished = false;
    //     }
    //     else
    //     {
    //         std::cout << "[CORO20fiber]: UNKNOWN " << m_type << "Task type"  << std::endl;
    //     }
    //     return *this;
    // }

    // 移动构造
    TaskCoro20& operator=(TaskCoro20&& other) {
        m_func_task = other.m_func_task;
        m_coro_task = other.m_coro_task;
        m_task = std::move(other.m_task);
        m_is_inited = other.m_is_inited;
        m_finished = other.m_finished;
        m_type = other.m_type;
        other.m_type = UNKNOWN;
        return *this;
    };

    TaskCoro20(TaskCoro20&& other)          
        : m_func_task(other.m_func_task), m_coro_task(other.m_coro_task), m_task(std::move(other.m_task))
        , m_is_inited(other.m_is_inited), m_finished(other.m_is_inited), m_type(other.m_type) {
            other.m_type = UNKNOWN;
        }
    


public:
    void reset(TaskCoro20&& other)
    {
        m_type = other.m_type;
        m_is_inited = other.m_is_inited;
        m_task = std::move(other.m_task);
    }

    void reset()
    {
        m_func_task = nullptr;
        m_coro_task = nullptr;
        m_is_inited = false;
        m_type = UNKNOWN;
    }

    void start()
    {
        // std::cout << "called start" << std::endl;
        if(m_type == CORO)
        {
            if(m_task.getHandle().done())
            {
                std::cout << "[coroutine]resume a finished handle" << std::endl;
            }
            m_task.getHandle().resume();
        }
        else if (m_type == FUNC)
        {
            m_finished = true;
            m_func_task();
        }
        else
        {
            std::cout << "[CORO20fiber]: UNKNOWN " << m_type << "Task type"  << std::endl;
        }
    }

    bool isFinished()       // 协程成功完成时返回true
    {
        if(! isLegal()) {return false;}
        bool is_finished = false;
        if(m_type == CORO)
        {
            is_finished = m_task.getHandle().done();
        }
        else if(m_type == FUNC)
        {
            is_finished = m_finished;
        }
        else 
        {
            is_finished = true;
        }
        return is_finished;
    }

    bool isLegal() { 
        bool taskState;
        if(m_type == CORO)
        {
            taskState = !!m_task.getHandle();
        }
        else if(m_type == FUNC)
        {
            taskState = !!m_func_task;
        }
        else 
        {
            taskState = false;
        }
        return m_is_inited && taskState;
     }

    void finally(std::function<void(Result<void>)>&& func)
    {
        m_task.finally(std::move(func));
    }

public:

    static TaskCoro20 create_coro(std::function<Task<void, TaskBeginExecuter>()> task)     // 协程任务
    {
        TaskCoro20 t;
        t.m_coro_task = task;
        // t.m_task = std::move(task());
        t.m_task = task();
        t.m_type = CORO;
        t.m_is_inited = true;
        return t;
    }  

    static TaskCoro20 create_func(std::function<void()> task)     // 协程任务
    {
        TaskCoro20 t;
        t.m_func_task = task;
        t.m_type = FUNC;
        t.m_is_inited = true;
        return t;
    }      

private:
    Task<void, TaskBeginExecuter> runner(std::function<void()>& func)
    {
        func();
        co_return;
    };

    enum Type
    {
        UNKNOWN = 0,
        CORO = 1,
        FUNC = 2,
    };

private:
    // 任务函数备份用作任务拷贝
    std::function<void()> m_func_task = nullptr;
    std::function<Task<void, TaskBeginExecuter>()> m_coro_task = nullptr;
    Task<void, TaskBeginExecuter> m_task;   // 严禁拷贝
    // 任务状态信息
    bool m_is_inited = true;            // 是否是可运行的协程
    bool m_finished = false;            // 普通函数任务是否运行完毕
    Type m_type = Type::UNKNOWN;                // 任务类型
};


}

