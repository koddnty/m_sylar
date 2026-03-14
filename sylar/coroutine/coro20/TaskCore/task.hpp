#pragma once
#include <condition_variable>
#include <iostream>
#include <coroutine>
#include <any>
#include <exception>
#include <functional>
#include <list>
#include <assert.h>
#include <mutex>
#include <optional>
#include <utility>
#include "result.hpp"

namespace m_sylar
{

class ITask {
public:
    virtual ~ITask() = default;
};



template<typename Executer>
class Task<void, Executer> : public ITask
{
public:
    using promise_type = TaskPromise<void, Executer>;

    Task() {}

    Task(std::coroutine_handle<promise_type> handle, Executer* executer, std::atomic<bool>* is_have_task = nullptr)        // 从promoise获得executer
        : m_handler(handle), m_executer(executer) {
            if(is_have_task)
            {
                m_is_have_task = is_have_task;
            }
        }

    // 禁用拷贝构造
    Task(Task& other) = delete;
    Task(const Task& other) = delete;
    Task& operator=(Task& other) = delete;

    // 移动构造
    Task(Task&& other)
        : m_handler(std::exchange(other.m_handler, {}))
        , m_executer(other.m_executer)
        , m_is_have_task(other.m_is_have_task) {
            other.m_is_have_task = nullptr;
            other.m_executer = nullptr;
        }

    Task<void, Executer>& operator=(Task<void, Executer>&& other)
    {
        // 清理自身资源
        if(m_is_have_task != nullptr)
        {
            *m_is_have_task = false;
        }
        if(m_handler && m_handler.done())
        {
            // std::cout << "--destory" << std::endl;
            m_handler.destroy();
        }

        // 资源移动
        // m_handler = std::move(other.m_handler);
        m_handler = std::exchange(other.m_handler, {});
        m_executer = std::move(other.m_executer);
        m_is_have_task = other.m_is_have_task;
        other.m_is_have_task = nullptr;
        other.m_executer = nullptr;
        return *this;
    }

    ~Task() override
    {
        if(m_is_have_task != nullptr)
        {
            *m_is_have_task = false;
        }
        if(m_handler && m_handler.done())
        {
            // std::cout << "--destory" << std::endl;
            m_handler.destroy();
        }
    }



public:
    void getResult()
    {
        // return m_handler.promise().getResult();
        return;
    }

    Task& then(std::function<void(Result<void>)> cb)
    {
        m_handler.promise().setCompleteCb(cb);
        return *this;
    }

    Task& catching(std::function<void(Result<void>)>&& func)
    {   // 发生异常后函数调用，获取resulta包装后的result
        m_handler.promise().setCompleteCb(
            [func](auto result)
            {
                try
                {
                    result.getOrThrow();
                }
                catch(std::exception& e)
                {
                    Result<void> r(e);
                    func(r);
                }
            }
        );
        return *this;
    }

    Task& finally(std::function<void(Result<void>)>&& func)
    {   // 发生异常后函数调用，获取resulta包装后的result
        m_handler.promise().setCompleteCb(
            [func](auto result)
            {
                try
                {
                    result.getOrThrow();
                    func(result);
                }
                catch(std::exception& e)
                {

                }
            }
        );
        return *this;
    }

    const std::coroutine_handle<promise_type>& getHandle() const
    {
        return m_handler;
    }

    Executer* getExecuter() const {return m_executer;}
private:
    std::coroutine_handle<promise_type> m_handler;
    Executer* m_executer;
    std::atomic<bool>* m_is_have_task = nullptr;
};
}
