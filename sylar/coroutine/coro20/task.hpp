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
#include "basic/log.h"
#include "basic/macro.h"
#include <utility>

namespace m_sylar
{
template<typename _ResultType, typename Executer>
class TaskAwaiter;
template<typename ResultType, typename Executer>
class TaskPromise;
template<typename Executer>
class InitialAwaiter;


class PTStateSync       // 用于Task和promisetype进行相互通信，状态同步
{
public:
    enum CoroutineState
    {
        RUNNING = 0,
        SUSPEND = 1,    // 当前为无效值，请勿使用
        FINISHED = 2    // 当且仅当协程状态为FINISHED时，Task才可以destory
    };

    enum TaskState
    {
        DESTRUCTED = 9,         // Task已经析构，需要promisetype退出时销毁协程帧
        EXIST = 8               // Task 存活，promiseType不要销毁协程帧
    };

    enum MSTATE
    {
        READY = -1,
        DISCONNECTED = -2
    };

public:
    PTStateSync(CoroutineState coroState, TaskState taskState)
        : m_coro_task(coroState), m_task_state(taskState) {}

    ~PTStateSync(){}

    inline CoroutineState getCoroState() const {return m_coro_task;}
    inline TaskState getTaskState() const {return m_task_state;}
    inline MSTATE getConnectState() const {return m_connect_state;}
    void setCoroState(CoroutineState coroState) {m_coro_task = coroState;}
    void setTaskState(TaskState taskState) {m_task_state = taskState;}
    void setConnectState(MSTATE connectState) {m_connect_state = connectState;}

    std::mutex& get_mutex() {return m_mutex;}

private:
    CoroutineState m_coro_task;
    TaskState m_task_state;
    MSTATE m_connect_state = READY;
    std::mutex m_mutex; // 强锁
    
};


// static std::atomic<int> count {0};
class FinalAwaiter
{   // 结束时由决定是否挂起或销毁
public:
    FinalAwaiter(PTStateSync* state)
        : m_state(state) {
        assert(m_state != nullptr);
    }

    bool await_ready() noexcept {
        std::unique_lock<std::mutex> lock(m_state->get_mutex());
        assert(m_state->getCoroState() != PTStateSync::FINISHED);
        m_state->setCoroState(PTStateSync::FINISHED);
        if(m_state->getTaskState() == PTStateSync::DESTRUCTED)
        {
            return true;
        }
        return false;
    }

    void await_suspend(std::coroutine_handle<> handle) noexcept { 
    }

    void await_resume() noexcept {}

private:
    // bool* m_is_have_task;       
    PTStateSync* m_state = nullptr;
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

    Task(std::coroutine_handle<promise_type> handle, Executer* executer, PTStateSync* state)        // 从promoise获得executer
        : m_handler(handle), m_executer(executer), m_state(state) {
            assert(m_state != nullptr);
            std::unique_lock<std::mutex> lock (m_state->get_mutex());
            m_state->setTaskState(PTStateSync::TaskState::EXIST);
        }

    // 禁用拷贝构造
    Task(Task& other) = delete;
    Task(const Task& other) = delete;
    Task& operator=(Task& other) = delete;

    // 移动构造
    Task(Task&& other)
        : m_handler(std::exchange(other.m_handler, {}))
        , m_executer(other.m_executer)
        , m_state(other.m_state) {
            other.m_state = nullptr;
            other.m_executer = nullptr;
        }
    Task<ResultType, Executer>& operator=(Task<ResultType, Executer>&& other)
    {
        if(this == &other) {return *this;}

        // 清理自身资源
        // M_SYLAR_ASSERT2(m_state != nullptr, "Task state(PTStateSync) is nullptr.");
        if(m_state)
        {   // 检测协程是否完成，如果未完成则仅设置Task析构状态，如果已经完成则destory
            std::unique_lock<std::mutex> lock (m_state->get_mutex());
            if(m_handler && m_handler.done() && m_state->getCoroState() == PTStateSync::FINISHED)
            {
                auto handler = m_handler;
                m_handler = nullptr;
                lock.unlock();  // 先释放锁，由于协程完成，此处不会受到promisetype干扰
                handler.destroy();
            }
            else
            {
                // 通知promisetype Task析构，需要自行销毁协程帧
                m_state->setTaskState(PTStateSync::TaskState::DESTRUCTED);
            }
        }

        // 资源移动
        m_handler = std::exchange(other.m_handler, {});
        m_executer = std::move(other.m_executer);
        m_state = other.m_state;
        other.m_state = nullptr;
        other.m_executer = nullptr;
        return *this;
    }

    ~Task() override
    {
        if(m_state)
        {   // 检测协程是否完成，如果未完成则仅设置Task析构状态，如果已经完成则destory
            std::unique_lock<std::mutex> lock (m_state->get_mutex());
            if(m_handler && m_handler.done() && m_state->getCoroState() == PTStateSync::FINISHED)
            {
                auto handler = m_handler;
                m_handler = nullptr;
                lock.unlock();  // 先释放锁，由于协程完成，此处不会受到promisetype干扰
                handler.destroy();
            }
            else
            {
                // 通知promisetype Task析构，需要自行销毁协程帧
                m_state->setTaskState(PTStateSync::TaskState::DESTRUCTED);
            }
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

    const std::coroutine_handle<promise_type>& getHandle() const
    {
        return m_handler;
    }

    Executer* getExecuter() const {return m_executer;}
private:
    std::coroutine_handle<promise_type> m_handler;
    Executer* m_executer;
    // std::atomic<bool>* m_is_have_task = nullptr;
    PTStateSync* m_state = nullptr;         // 如果不为空，则应指向promise_type管理的变量，表明是否有外部task正在使用当前协程
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
        // m_is_have_task = true;
        return Task<ResultType, Executer>(std::coroutine_handle<TaskPromise>::from_promise(*this), m_executer, &m_state);
    }

    InitialAwaiter<Executer> initial_suspend() {        // 直接由execute调用
        return InitialAwaiter<Executer>(m_executer);
    }


    FinalAwaiter final_suspend() noexcept {
        return FinalAwaiter(&m_state);
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
    TaskAwaiter<_ResultType, _Executer> await_transform (Task<_ResultType, _Executer>&& task)   // 用于子协程管理
    {   // 需要控制子任务生命周期
        // return TaskAwaiter<_ResultType, _Executer>(std::move(task));
        // auto* m_child_task = std::any_cast<Task<_ResultType, _Executer>>(&task);
        Task<_ResultType, _Executer>* task_ptr = new Task<_ResultType, _Executer>(std::move(task));
        m_child_task_ptr.reset(task_ptr);
        TaskAwaiter<_ResultType, _Executer> child_task (task_ptr);
        return child_task;
    }

    template<typename AwaiterImpl>      // 用作co_await awaiter
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
            return;
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
    // std::atomic<bool> m_is_have_task {false};
    PTStateSync m_state{PTStateSync::CoroutineState::RUNNING, PTStateSync::TaskState::EXIST};
};



template<typename _ResultType>
class Awaiter
{   // Awaiter抽象类，用于co_awaiter，并带有自动的句柄管理
public:
    Awaiter()
    {}

    ~Awaiter()
    {}

    Awaiter(Awaiter& other)
    {
        m_handle = other.m_handle;
        m_executer = other.m_executer;
    }

    Awaiter(Awaiter&& other) = delete;

public:
    virtual bool await_ready()
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
        bool expect = false;
        if(m_is_resumed.compare_exchange_strong(expect, true))
        {
            // m_is_resumed = true;
            if(m_handle.done()){
                return;
            }
            dispatch([this, value]() {
                // 将 value 封装到 _result 当中，await_resume 时会返回 value
                m_result = Result<_ResultType>(static_cast<_ResultType>(value));
                if(!m_handle.done())
                {
                    m_handle.resume();
                }
                else
                {
                    std::cout << "原位置会导致resumedestoryed的协程" << std::endl;
                }
            });
        }

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
    std::atomic<bool> m_is_resumed = false;
};



template<typename _ResultType, typename _Executer>
class TaskAwaiter : public Awaiter<_ResultType>
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
    void on_suspend() override
    {   // 控制子协程是否完成后恢复自己
        // m_task.getExecuter()->execute([handle, this](){
        m_task->finally([this](Result<_ResultType> result){
            this->resume(m_task->getResult());
        });
        // });
    }

    // _ResultType await_resume()
    // {
    //     return m_task->getResult();
    // }

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

    Awaiter(Awaiter& other) = default;
    Awaiter(Awaiter&& other) = delete;

public:
    virtual bool await_ready()
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



template<typename Executer>
class Task<void, Executer> : public ITask
{
public:
    using promise_type = TaskPromise<void, Executer>;

    Task() {}

    Task(std::coroutine_handle<promise_type> handle, Executer* executer, PTStateSync* state)        // 从promoise获得executer
        : m_handler(handle), m_executer(executer), m_state(state) {
            assert(m_state != nullptr);
            std::unique_lock<std::mutex> lock (m_state->get_mutex());
            m_state->setTaskState(PTStateSync::TaskState::EXIST);
        }

    // 禁用拷贝构造
    Task(Task& other) = delete;
    Task(const Task& other) = delete;
    Task& operator=(Task& other) = delete;

    // 移动构造
    Task(Task&& other)
        : m_handler(std::exchange(other.m_handler, {}))
        , m_executer(other.m_executer)
        , m_state(other.m_state) {
            other.m_state = nullptr;
            other.m_executer = nullptr;
        }

    Task<void, Executer>& operator=(Task<void, Executer>&& other)
    {
        if(this == &other) {return *this;}
        // 清理自身资源
        // M_SYLAR_ASSERT2(m_state != nullptr, "Task state(PTStateSync) is nullptr.");
        if(m_state)
        {   // 检测协程是否完成，如果未完成则仅设置Task析构状态，如果已经完成则destory
            std::unique_lock<std::mutex> lock (m_state->get_mutex());
            if(m_handler && m_handler.done() && m_state->getCoroState() == PTStateSync::FINISHED)
            {
                auto handler = m_handler;
                m_handler = nullptr;
                lock.unlock();  // 先释放锁，由于协程完成，此处不会受到promisetype干扰
                handler.destroy();
            }
            else
            {
                // 通知promisetype Task析构，需要自行销毁协程帧
                m_state->setTaskState(PTStateSync::TaskState::DESTRUCTED);
            }
        }

        // 资源移动
        m_handler = std::exchange(other.m_handler, {});
        m_executer = std::move(other.m_executer);
        m_state = other.m_state;
        other.m_state = nullptr;
        other.m_executer = nullptr;
        return *this;
    }

    ~Task() override
    {
        if(m_state)
        {   // 检测协程是否完成，如果未完成则仅设置Task析构状态，如果已经完成则destory
            std::unique_lock<std::mutex> lock (m_state->get_mutex());
            if(m_handler && m_handler.done() && m_state->getCoroState() == PTStateSync::FINISHED)
            {
                auto handler = m_handler;
                m_handler = nullptr;
                lock.unlock();  // 先释放锁，由于协程完成，此处不会受到promisetype干扰
                handler.destroy();
            }
            else
            {
                // 通知promisetype Task析构，需要自行销毁协程帧
                m_state->setTaskState(PTStateSync::TaskState::DESTRUCTED);
            }
        }
    }


public:
    void getResult()
    {
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
    // std::atomic<bool>* m_is_have_task = nullptr;
    PTStateSync* m_state = nullptr;
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
        // m_is_have_task = new bool;
        auto handle = std::coroutine_handle<TaskPromise>::from_promise(*this);
        // std::cout << "create a handle = " << &handle << std::endl;
        return Task<void, Executer>(handle, m_executer, &m_state);
    }

    InitialAwaiter<Executer> initial_suspend() {        // 直接由execute调用
        return InitialAwaiter<Executer>(m_executer);
    }

    FinalAwaiter final_suspend() noexcept {
        return FinalAwaiter(&m_state);
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
            return;
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
    PTStateSync m_state{PTStateSync::CoroutineState::RUNNING, PTStateSync::TaskState::EXIST};
    // bool* m_is_have_task = nullptr;
    // std::atomic<bool> m_is_have_task {false};
    // std::atomic<State> m_state = RUNNING;
};


template<typename _Executer>
class TaskAwaiter<void, _Executer> : public Awaiter<void>
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


    void on_suspend() override
    {   // 控制子协程是否完成后恢复自己
        // m_task.getExecuter()->execute([handle, this](){
        m_task->finally([this](Result<void> result){
            this->resume();
        });
        // });
    }



private:
    Task<void, _Executer>* m_task;       // 子协程任务
};




}