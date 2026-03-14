#pragma once
#include <iostream>
#include <list>
#include <shared_mutex>
#include <atomic>
#include <memory>
#include <sys/epoll.h>
#include "task.hpp"
#include "fiber.h"



namespace m_sylar
{

class FdContextManager;
// fd_ctx
class FdContext : public std::enable_shared_from_this<FdContext>
{
public:
    using ptr = std::shared_ptr<FdContext>;
    enum Event
    {   
        NONE = 0x0,
        READ = EPOLLIN,
        WRITE = EPOLLOUT
    };

public:
    FdContext(int fd);
    ~FdContext();

    FdContext& addEvent(Event event, TaskCoro20&& task);        // 原基础上添加event
    FdContext& delEvent(Event event);         // 删除event
    FdContext& trigger(Event event);       
    inline int getFd() const {return m_fd; }
    inline Event getEvent() const {return m_event; }
    inline bool hasEvent() const {return m_event;}   // 是否存在事件

    FdContext&  reset();           // 清空所有事件，回归初始状态


private:
    int m_fd;
    Event m_event = NONE;
    TaskCoro20 m_cb_read {};
    TaskCoro20 m_cb_write {};
};



class FdContextManager : std::enable_shared_from_this<FdContextManager>
{
public:
    using ptr = std::shared_ptr<FdContextManager>;
    enum State
    {
        INIT = 0,       // FdContext为空，即不监听
        READY = 1,      // FdContext监听中但任务列表为空，需注册线程自行完成任务
        BUSY = 2,       // FdContext监听中，同时已有线程在运行注册任务，此时仅需要注册任务
        LOCK = 3,       // FdContext监听中，注册任务运行完成，运行线程准备退出需等待完成后当前线程才能注册任务。
        ERROR = 4       // 状态混乱，抛出异常
    };

    // 任务事件，需要由this负责运行时机
    class RegistedTask;        // 基任务类型
    class DEL_TASK;                // 删除事件回调任务
    class ADD_TASK;                // 添加事件回调任务
    class TRIGGER_TASK;            // 触发事件回调任务

public:
    FdContextManager(int fd);
    ~FdContextManager();

    /**
        @brief 添加事件, 返回true表示已经执行，false表示正在执行或等待执行
    */
    bool addTask(std::shared_ptr<RegistedTask> task);

    void trigger(FdContext::Event event, std::function<void(FdContext::ptr, FdContext::Event )> cb);   // 触发回调
    void addEvent(FdContext::Event event, TaskCoro20&& task, std::function<void(FdContext::ptr, FdContext::Event )> cb);
    void delEvent(FdContext::Event event, std::function<void(FdContext::ptr, FdContext::Event )> cb);


private:
    /**
        @brief 添加事件的线程需要执行所有事件, 返回true只保证完成调用前存在的task   
    */
    void solveTasks();
    bool solveSingleTask();


private:
    std::atomic<State> m_state {INIT};
    std::list<std::shared_ptr<RegistedTask>> m_tasks;       // 任务列表
    std::shared_mutex m_task_mutex;
    std::shared_ptr<FdContext> m_fdcontex;
};





// 事件
class FdContextManager::RegistedTask
{
public:
    using ptr = std::shared_ptr<RegistedTask>;
    RegistedTask(FdContextManager::ptr fd_ctx_manager, std::function<void(FdContext::ptr, FdContext::Event )> m_cb);
    virtual ~RegistedTask();

    virtual void run() = 0;         // 任务执行函数

    inline FdContextManager::ptr getFdCtxMgr() const {return m_fd_ctx_manager; }
    // void setCb(std::function<void(FdContext::ptr, FdContext::Event )>);   // 在完成基本事件后触发，用于epollctl或其他模型的状态同步

protected:    
    FdContextManager::ptr m_fd_ctx_manager;
    std::function<void(FdContext::ptr, FdContext::Event)> m_cb;     // 状态同步函数
};


class FdContextManager::DEL_TASK : public FdContextManager::RegistedTask
{
public:
    using ptr = std::shared_ptr<DEL_TASK>;
    DEL_TASK(FdContextManager::ptr fd_ctx_manager, FdContext::Event event, std::function<void(FdContext::ptr, FdContext::Event )> m_cb);

    void run() override;


private:
    FdContext::Event m_event;
};


class FdContextManager::ADD_TASK : public FdContextManager::RegistedTask
{
public:
    using ptr = std::shared_ptr<ADD_TASK>;
    ADD_TASK(FdContextManager::ptr fd_ctx_manager, TaskCoro20&& task, FdContext::Event event, std::function<void(FdContext::ptr, FdContext::Event)> m_cb);

    void run() override;

private:   
    FdContext::Event m_event;
    TaskCoro20 m_task;
};

class FdContextManager::TRIGGER_TASK : public FdContextManager::RegistedTask
{
public:
    using ptr = std::shared_ptr<TRIGGER_TASK>;
    TRIGGER_TASK(FdContextManager::ptr fd_ctx_manager, FdContext::Event event, std::function<void(FdContext::ptr, FdContext::Event )> m_cb);

    void run() override;
    
private:   
    FdContext::Event m_event;
};

}