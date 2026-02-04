#pragma once
#include <condition_variable>
#include <coroutine>
#include <exception>
#include <functional>
#include <list>
#include <mutex>
#include <optional>
#include <utility>

namespace m_sylar
{
template<typename _ResultType>
class TaskAwaiter;
template<typename ResultType>
class TaskPromise;


template<typename ResultType>
class Result
{
public: 
    Result(ResultType&& result)
        : m_result(std::move(result)) {}

    Result(const std::exception_ptr& e)
        : m_exception(e) {}

    ResultType& getOrThrow()
    {
        if(m_exception)
        {
            std::rethrow_exception(m_exception);
        }
        return m_result;
    }

private:
    ResultType m_result;
    std::exception_ptr m_exception;
};


template<typename ResultType>
class Task
{
public: 
    using promise_type = TaskPromise<ResultType>;

    Task(std::coroutine_handle<promise_type> handle)
        : m_handler(handle) {}

    Task(Task&& other)
        : m_handler(std::exchange(other.m_handler, {})) {}

    // 禁用拷贝构造
    Task(Task& other) = delete;
    Task& operator=(Task& other) = delete;

    ~Task()
    {
        if(m_handler)
        {
            m_handler.destroy();
        }
    }

public:
    ResultType getResult()
    {
        return m_handler.promise().getResult();
    }

    Task& then(std::function<void(Result<ResultType>)> cb)
    {   
        m_handler.promise().setCompleteCb(cb);
        return *this;
    }

    Task& catching(std::function<void(Result<ResultType>)>&& func)
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
                    func(Result<ResultType>(e));   
                }
            }
        );
        return *this;
    }

    Task& finally(std::function<void(Result<ResultType>)>&& func)
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

private:
    std::coroutine_handle<promise_type> m_handler;
};


template<typename ResultType>
class TaskPromise
{
public:
    Task<ResultType> get_return_object()
    {
        return Task<ResultType>(std::coroutine_handle<TaskPromise>::from_promise(*this));
    }

    std::suspend_never initial_suspend() { return {};}
    std::suspend_always final_suspend() noexcept { return {};}

    void unhandled_exception()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_result = Result<ResultType>(std::current_exception());
        m_completion.notify_all();
        notifyAllCbs();
    }

    void return_value(ResultType value)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_result = Result<ResultType>(std::move(value));
        m_completion.notify_all();
        notifyAllCbs();
    }

    template<typename _ResultType>  // co_await协程返回值类型，与ResultType 有所区分
    TaskAwaiter<_ResultType> await_transform (Task<_ResultType>&& task) 
    {   // 需要控制子任务生命周期
        return TaskAwaiter<_ResultType>(std::move(task));
    }

public: 
    ResultType getResult()      // 获取协程运行结果
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if(!m_result.has_value())
        {   
            m_completion.wait(lock);
        }
        return m_result.value().getOrThrow();
    }

    void setCompleteCb(std::function<void(Result<ResultType>)>&& cb)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if(m_result.has_value())
        {
            lock.unlock();
            cb(m_result.value());
        }
        m_callbacks.push_back(cb);
    }


private:

    void notifyAllCbs()
    {
        Result<ResultType> value = m_result.value();
        for(auto& it : m_callbacks)
        {
            it(value);
        }
        m_callbacks.clear();
    }

private:
    std::optional<Result<ResultType>> m_result;
    std::mutex m_mutex;
    std::condition_variable m_completion;

    std::list<std::function<void(Result<ResultType>)>> m_callbacks;

};


template<typename _ResultType>
class TaskAwaiter
{
public: 
    TaskAwaiter(Task<_ResultType>&& task)
        : m_task(std::move(task)) {}

public: 
    virtual bool await_ready()
    {
        return false;
    } 

    virtual void await_suspend(std::coroutine_handle<TaskPromise<_ResultType>> handle)
    {
        // 实现协程恢逻辑
        m_task.finally([handle](Result<_ResultType> result){
            handle.resume();
        });
    }

    _ResultType await_resume()
    {
        return m_task.getResult();
    }



private:    
    Task<_ResultType> m_task;
};
}