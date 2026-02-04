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


class TaskCoro20
{
public:
    using HandlePtr = std::shared_ptr<std::coroutine_handle<>>;
    using ptr = std::shared_ptr<TaskCoro20>;

    TaskCoro20() {m_status = UNSET;}
    TaskCoro20(std::function<Task<>()> cb);
    TaskCoro20(std::function<void()> cb);
    TaskCoro20(HandlePtr handle);
    ~TaskCoro20();         

    void setTask(std::function<Task<>()> cb);
    void setTask(std::function<void()> cb);
    void setHandle(HandlePtr handle);       

    void reset();
    void resume();
    
    bool isLegal();

private:
    enum Status
    {
        UNSET = -1,         // 默认构造状态
        INIT = 0,           // 未进行调用过，无handle
        SUSPEND = 1,        // handle已经注册
        EXEC = 2,           // 运行中
        TERM = 3            // 结束
    };

    // Task<> FiberFunc(std::function<void()> cb);

    void task()
private:
    std::function<Task<>()> m_task;
    Status m_status;            // 协程状态
    HandlePtr m_handler;        // 协程句柄 
};

}

