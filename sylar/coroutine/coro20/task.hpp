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

namespace m_sylar
{
template<typename _ResultType, typename Executer>
class TaskAwaiter;
template<typename ResultType, typename Executer>
class TaskPromise;
template<typename Executer>
class InitialAwaiter;


class FinalAwaiter
{   // 结束时由决定是否挂起或销毁
public:
    FinalAwaiter(bool* is_have_task)
        : m_is_have_task(is_have_task) { }

    bool await_ready() noexcept {
        // if(!m_is_have_task)
        // {
        //     return true;
        // }
        // assert(m_is_have_task);
        // std::cout << "have task ?:"<< *m_is_have_task << std::endl;
        // std::cout << "returnValue:" << !(*m_is_have_task) << std::endl;
        return !(*m_is_have_task);
    }

    void await_suspend(std::coroutine_handle<> handle) noexcept { }

    void await_resume() noexcept {}

private:
    bool* m_is_have_task;       
};


class ITask {
public:
    virtual ~ITask() = default;
};


class AbstractExecuter
{   // 负责调度协程恢复函数
public:
    AbstractExecuter() = default;
    virtual ~AbstractExecuter() {}

    virtual void execute(std::function<void()> &&func)
    {

        func();
    }

    virtual void initialExecute(std::function<void()> &&func)
    {
        func();
    }
};

class BaseExecutor : public AbstractExecuter
{
public:
    virtual void execute(std::function<void()>&& func) override
    {   // 协程中间挂起恢复逻辑
        func();
    }

    virtual void initialExecute(std::function<void()> &&func) override
    {   // 协程启动时恢复逻辑
        func();
    }
};


template<typename ResultType>
class Result
{
public:
    Result(ResultType&& result)
        : m_result(std::move(result)) {}

    Result(const std::exception_ptr& e)
        : m_exception(e) {}

    Result(const std::exception& e)
        : m_exception(std::make_exception_ptr(e)) {}

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


template<typename ResultType, typename Executer = BaseExecutor>
class Task : public ITask
{
public:
    using promise_type = TaskPromise<ResultType, Executer>;

    Task() {}

    Task(std::coroutine_handle<promise_type> handle, Executer* executer, bool* is_have_task = nullptr)        // 从promoise获得executer
        : m_handler(handle), m_executer(executer) {
            if(is_have_task)
            {
                m_is_have_task = is_have_task;
            }
        }

    Task(Task&& other)
        : m_handler(std::exchange(other.m_handler, {}))
        , m_executer(other.m_executer)
        , m_is_have_task(other.m_is_have_task) {}

    // 禁用拷贝构造
    Task(Task& other) = delete;
    Task& operator=(Task& other) = delete;

    ~Task() override
    {
        if(m_is_have_task != nullptr)
        {
            *m_is_have_task = false;
        }
        if(m_handler && m_handler.done())
        {
            m_handler.destroy();
        }
    }

    Task<ResultType, Executer>& operator=(Task<ResultType, Executer>&& other)
    {
        m_handler = std::move(other.m_handler);
        m_executer = std::move(other.m_executer);
        m_is_have_task = other.m_is_have_task;
        return *this;
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

    const std::coroutine_handle<promise_type>& getHandle() const
    {
        return m_handler;
    }

    Executer* getExecuter() const {return m_executer;}
private:
    std::coroutine_handle<promise_type> m_handler;
    Executer* m_executer;
    bool* m_is_have_task = nullptr;             // 如果不为空，则应指向promise_type管理的变量，表明是否有外部task正在使用当前协程
};


template<typename ResultType, typename Executer>
class TaskPromise
{
public:
    ~TaskPromise()
    {

        delete m_executer;
    }

    Task<ResultType, Executer> get_return_object()
    {

        m_executer = new Executer();    // taskpromise负责execute控制
        // Task<ResultType, Executer> task (std::coroutine_handle<TaskPromise>::from_promise(*this), m_executer);
        // m_task = &task;
        m_is_have_task = true;
        return Task<ResultType, Executer>(std::coroutine_handle<TaskPromise>::from_promise(*this), m_executer, &m_is_have_task);
    }

    InitialAwaiter<Executer> initial_suspend() {        // 直接由execute调用
        return InitialAwaiter<Executer>(m_executer);
    }


    FinalAwaiter final_suspend() noexcept {
        return FinalAwaiter(&m_is_have_task);
    }

    void unhandled_exception()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_result = Result<ResultType>(std::current_exception());
        lock.unlock();
        m_completion.notify_all();
        notifyAllCbs();
    }

    void return_value(ResultType value)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_result = Result<ResultType>(std::move(value));
        lock.unlock();
        m_completion.notify_all();
        notifyAllCbs();
    }

    template<typename _ResultType, typename _Executer>  // co_await协程返回值类型，与ResultType 有所区分
    TaskAwaiter<_ResultType, _Executer> await_transform (Task<_ResultType, _Executer>&& task)
    {   // 需要控制子任务生命周期
        // return TaskAwaiter<_ResultType, _Executer>(std::move(task));
        // auto* m_child_task = std::any_cast<Task<_ResultType, _Executer>>(&task);
        Task<_ResultType, _Executer>* task_ptr = new Task<_ResultType, _Executer>(std::move(task));
        m_child_task_ptr.reset(task_ptr);
        TaskAwaiter<_ResultType, _Executer> child_task (task_ptr);
        return child_task;
    }

    template<typename AwaiterImpl>
    AwaiterImpl await_transform(AwaiterImpl awaiter)
    {   // 为传入的awaiter设置调度器
        awaiter.install_executor(m_executer);

        return awaiter;
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
    Executer* m_executer;
    // ITask m_child_task;
    std::unique_ptr<ITask> m_child_task_ptr;
    std::list<std::function<void(Result<ResultType>)>> m_callbacks;
    bool m_is_have_task = false;

};



template<typename _ResultType>
class Awaiter
{
public:
    Awaiter()
    {

    }

    ~Awaiter()
    {

    }

    bool await_ready()
    {
        return false;
    }

    // TaskPromise<_ResultType, _Executer>
    void await_suspend(std::coroutine_handle<> handle)
    {   // 控制协程是否完成后恢复自己
        m_handle = handle;
        on_suspend();
    }

    _ResultType await_resume()
    {
        // if (!m_result.has_value()) {
        //     throw std::runtime_error("No result available");
        // }
        before_resume();
        return m_result->getOrThrow();
    }

    void install_executor(AbstractExecuter* executer)
    {
        m_executer = executer;
    }

    void dispatch(std::function<void()> &&func)
    {
        if(m_executer)
        {
            m_executer->execute(std::move(func));
        }
        else
        {
            func();
        }
    }

protected:
    virtual void on_suspend()
    {}

    virtual void before_resume()
    {}

protected:
    // 结果对子类可见，方便灵活操作
    std::optional<Result<_ResultType>> m_result{};

    void resume(_ResultType value) {
        if(m_handle.done()){

        }
        dispatch([this, value]() {
            // 将 value 封装到 _result 当中，await_resume 时会返回 value
            m_result = Result<_ResultType>(static_cast<_ResultType>(value));

            m_handle.resume();
        });
    }

    void resume_unsafe() {
        dispatch([this]() {

            m_handle.resume();
        });
    }

    void resume_exception(std::exception_ptr&& e)
    {
        dispatch([this, e](){
            m_result = Result<_ResultType>(e);

            m_handle.resume();
        });
    }

private:
    std::coroutine_handle<> m_handle;
    AbstractExecuter* m_executer = NULL;
};



template<typename _ResultType, typename _Executer>
class TaskAwaiter
{   // co_await Task时获得的awaiter,由子任务模板构成，在子任务运行完毕后会恢复当前任务
public:
    TaskAwaiter(Task<_ResultType, _Executer>* task)
        : m_task(task) {

    }

    ~TaskAwaiter()
    {

    }


public:
    bool await_ready()
    {
        return false;
    }

    // TaskPromise<_ResultType, _Executer>
    void await_suspend(std::coroutine_handle<> handle)
    {   // 控制子协程是否完成后恢复自己
        // m_task.getExecuter()->execute([handle, this](){
        m_task->finally([handle](Result<_ResultType> result){

            handle.resume();
        });
        // });
    }

    _ResultType await_resume()
    {
        return m_task->getResult();
    }

private:
    Task<_ResultType, _Executer>* m_task;           // 子协程任务
    // _Executer* m_executer;                       // 调度器
};



template<typename Executer>
class InitialAwaiter
{   // 开始时由execute决定是否挂起
public:
    InitialAwaiter(Executer* executer)
        : m_executer(executer) {}

    bool await_ready(){ return false; }

    void await_suspend(std::coroutine_handle<> handle)
    {
        m_executer->initialExecute([handle](){

            handle.resume();
        });
    }

    void await_resume() {}

private:
    Executer* m_executer;
};





// void 篇特化
template<>
class Result<void>
{
public:
    Result() {}

    Result(const std::exception_ptr& e)
        : m_exception(e) {}

    Result(const std::exception& e)
        : m_exception(std::make_exception_ptr(e)) {}

    void getOrThrow()
    {
        if(m_exception)
        {
            std::rethrow_exception(m_exception);
        }
    }

private:
    std::exception_ptr m_exception;
};


template<typename Executer>
class Task<void, Executer> : public ITask
{
public:
    using promise_type = TaskPromise<void, Executer>;

    Task() {}

    Task(std::coroutine_handle<promise_type> handle, Executer* executer, bool* is_have_task = nullptr)        // 从promoise获得executer
        : m_handler(handle), m_executer(executer) {
            if(is_have_task)
            {
                m_is_have_task = is_have_task;
            }
        }

    Task(Task&& other)
        : m_handler(std::exchange(other.m_handler, {}))
        , m_executer(other.m_executer)
        , m_is_have_task(other.m_is_have_task) {}

    // 禁用拷贝构造
    Task(Task& other) = delete;
    Task& operator=(Task& other) = delete;

    ~Task() override
    {
        if(m_is_have_task != nullptr)
        {
            *m_is_have_task = false;
        }
        if(m_handler && m_handler.done())
        {
            m_handler.destroy();
        }
    }


    Task<void, Executer>& operator=(Task<void, Executer>&& other)
    {
        m_handler = std::move(other.m_handler);
        m_executer = std::move(other.m_executer);
        m_is_have_task = other.m_is_have_task;
        return *this;
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
    bool* m_is_have_task = nullptr;
};


template<typename Executer>
class TaskPromise<void, Executer>
{
public:
    ~TaskPromise()
    {

        delete m_executer;
    }

    Task<void, Executer> get_return_object()
    {

        m_executer = new Executer();    // taskpromise负责execute控制
        m_is_have_task = true;
        return Task<void, Executer>(std::coroutine_handle<TaskPromise>::from_promise(*this), m_executer, &m_is_have_task);
    }

    InitialAwaiter<Executer> initial_suspend() {        // 直接由execute调用
        return InitialAwaiter<Executer>(m_executer);
    }

    FinalAwaiter final_suspend() noexcept {
        // std::cout << "final_suspend: " << this << std::endl;
        return FinalAwaiter(&m_is_have_task);
    }

    void unhandled_exception()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_result = Result<void>(std::current_exception());
        lock.unlock();
        m_completion.notify_all();
        notifyAllCbs();
    }

    void return_void()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_result = Result<void>();
        lock.unlock();
        m_completion.notify_all();
        notifyAllCbs();
    }

    template<typename _ResultType, typename _Executer>  // co_await协程返回值类型，与ResultType 有所区分
    TaskAwaiter<_ResultType, _Executer> await_transform (Task<_ResultType, _Executer>&& task)
    {   // 需要控制子任务生命周期
        Task<_ResultType, _Executer>* task_ptr = new Task<_ResultType, _Executer>(std::move(task));
        m_child_task_ptr.reset(task_ptr);
        TaskAwaiter<_ResultType, _Executer> child_task (task_ptr);
        return child_task;
        // return TaskAwaiter<_ResultType, _Executer>(std::move(task));
    }

    template<typename AwaiterImpl>
    AwaiterImpl await_transform(AwaiterImpl awaiter)
    {   // 为传入的awaiter设置调度器
        awaiter.install_executor(m_executer);
        return awaiter;
    }



public:
    void getResult()      // 获取协程运行结果
    {
    }

    void setCompleteCb(std::function<void(Result<void>)>&& cb)
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
        Result<void> value = m_result.value();
        for(auto& it : m_callbacks)
        {
            it(value);
        }
        m_callbacks.clear();
    }

private:
    std::optional<Result<void>> m_result;
    std::mutex m_mutex;
    std::condition_variable m_completion;
    Executer* m_executer;
    std::unique_ptr<ITask> m_child_task_ptr;
    std::list<std::function<void(Result<void>)>> m_callbacks;
    bool m_is_have_task = false;
};


template<typename _Executer>
class TaskAwaiter<void, _Executer>
{
public:
    TaskAwaiter(Task<void, _Executer>* task)
        : m_task(task) {

    }
    ~TaskAwaiter()
    {

    }

public:
    bool await_ready()
    {
        return false;
    }

    // TaskPromise<_ResultType, _Executer>
    void await_suspend(std::coroutine_handle<> handle)
    {   // 控制协程是否完成后恢复自己
        // m_task.getExecuter()->execute([handle, this](){
            // handle.resume();
        m_task->finally([handle](Result<void> result){

            handle.resume();
        });
        // });
    }

    void await_resume()
    {
        m_task->getResult();
    }

private:
    Task<void, _Executer>* m_task;       // 子协程任务
    // _Executer* m_executer;                   // 调度器
};


template<>
class Awaiter<void>
{
public:
    Awaiter()
    {

    }

    ~Awaiter()
    {

    }

    bool await_ready()
    {
        return false;
    }

    // TaskPromise<_ResultType, _Executer>
    void await_suspend(std::coroutine_handle<> handle)
    {   // 控制协程是否完成后恢复自己
        m_handle = handle;
        on_suspend();

    }

    void await_resume()
    {
        // if (!m_result.has_value()) {
        //     throw std::runtime_error("No result available");
        // }
        before_resume();
        m_result->getOrThrow();
    }

    void install_executor(AbstractExecuter* executer)
    {
        m_executer = executer;
    }

    void dispatch(std::function<void()> &&func)
    {
        if(m_executer)
        {
            m_executer->execute(std::move(func));
        }
        else
        {
            func();
        }
    }

protected:
    virtual void on_suspend()
    {}

    virtual void before_resume()
    {}

protected:
    // 结果对子类可见，方便灵活操作
    std::optional<Result<void>> m_result{};

    void resume() {

        dispatch([this]() {
            // 将 value 封装到 _result 当中，await_resume 时会返回 value

            m_handle.resume();
        });
    }

    void resume_unsafe() {
        dispatch([this]() {

            m_handle.resume();
        });
    }

    void resume_exception(std::exception_ptr&& e)
    {
        dispatch([this, e](){
            m_result = Result<void>(e);

            m_handle.resume();
        });
    }

private:
    std::coroutine_handle<> m_handle;
    AbstractExecuter* m_executer = NULL;
};

}