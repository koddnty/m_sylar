#pragma once
#include <cstdlib>
#include <functional>
#include <coroutine>
#include <memory>
#include <coroutine>
#include "basic/log.h"


namespace m_sylar
{

class RTValue
{
public:
    class promise_type
    {
    public:
        RTValue get_return_object() {
            return {};
        }       // 1
        std::suspend_never initial_suspend () {
            return {};
        }      // 2 
        std::suspend_never final_suspend() noexcept {
            return {};
        }        // 7
        void unhandled_exception() {}       
        
        void return_void() { 
        };          // 6
    };
};


class TaskCoro20
{
public:
    using HandlePtr = std::shared_ptr<std::coroutine_handle<>>;
    using ptr = std::shared_ptr<TaskCoro20>;

    TaskCoro20() {m_status = UNSET;}
    TaskCoro20(std::function<void()> cb);
    TaskCoro20(HandlePtr handle);
    ~TaskCoro20();         

    void setTask(std::function<void()> cb);
    void setHandle(HandlePtr handle);       

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

    RTValue FiberFunc(std::function<void()> cb);
private:
    std::function<void()> m_task;
    Status m_status;        // 协程状态
    HandlePtr m_handler;      // 协程句柄 
};

}

