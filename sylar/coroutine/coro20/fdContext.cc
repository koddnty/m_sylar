#include "fdContext.h"
#include <string.h>

namespace m_sylar
{

static Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");


// FdContext
FdContext::FdContext(int fd, int epollFd)
{
    m_fd = fd;
    m_epollFd = epollFd;
}

FdContext::~FdContext()
{}

FdContext& FdContext::addEvent(Event event, TaskCoro20&& task)
{
    // m_event = Event(m_event | event);
    // if(event & READ)
    // {
    //     m_cb_read = std::move(task);
    // }
    // if(event & WRITE)
    // {
    //     m_cb_write = std::move(task);
    // }
    // return *this;
    bool is_have = false;
    Event total_event = event;
    // 检查原有事件
    if(hasEvent())
    {
        is_have = true;
        total_event = (Event)(event | m_event);
    }
    
    // epoll 事件组装
    epoll_event new_event;
    new_event.events = total_event | EPOLLET;
    new_event.data.fd = m_fd;
    int rt = 0;
    if(is_have)
    {
        rt = epoll_ctl(m_epollFd, EPOLL_CTL_MOD, m_fd, &new_event);
    }
    else 
    {
        rt = epoll_ctl(m_epollFd, EPOLL_CTL_ADD, m_fd, &new_event);
    }
    if(rt)
    {   // 错误检查
        M_SYLAR_LOG_ERROR(g_logger) << "[IOManager]faield to addEvent"
                                    << "errno=" << errno << " error:" << strerror(errno);
        return *this;
    }
    rt = 0;
    
    
    // fdcontex 设置
    m_event = total_event;
    if(event & Event::READ)
    {
        m_cb_read = std::move(task);
    }
    if(event & Event::WRITE)
    {
        m_cb_write = std::move(task);
    }
    return *this;
}

FdContext& FdContext::delEvent(Event event)
{
    m_event = Event(m_event & ~event);
    if(!(m_event & READ))
    {
        m_cb_read.reset();
    }
    if(!(m_event & WRITE))
    {
        m_cb_write.reset();
    }
    return *this;
}

FdContext& FdContext::trigger(Event event)
{
    Event merge = Event(m_event & event);
    if(merge & READ)
    {
        auto t = m_cb_read.clone();
        if(!t.isLegal())
        {
            M_SYLAR_LOG_ERROR(g_logger) << "task(TaskCoro20) is illegal";
        }
        else
        {
            t.start();
        }
    }
    if(merge & WRITE)
    {
        auto t = m_cb_write.clone();
        if(!t.isLegal())
        {
            M_SYLAR_LOG_ERROR(g_logger) << "task(TaskCoro20) is illegal";
        }
        else
        {
            t.start();
        }
    }
    return *this;
}

inline int FdContext::getFd()
{
    return m_fd;
}

inline bool FdContext::hasEvent()
{
    return m_event;
}  

FdContext& FdContext::reset()
{
    m_cb_read.reset();
    m_cb_write.reset();
    m_event = NONE;
    return *this;
}   
    


// FdContextManager
FdContextManager::FdContextManager(int fd, int epollFd)
{
    m_fdcontex = std::make_shared<FdContext>(fd, epollFd);
    m_fdcontex->reset();
}

FdContextManager::~FdContextManager()
{}

bool FdContextManager::addTask(std::shared_ptr<RegistedTask> task)
{
    // M_SYLAR_ASSERT2(m_state == INIT, "")
    // 事件注册
    std::unique_lock<std::shared_mutex> wlock(m_task_mutex);
    m_tasks.push_back(task);
    wlock.unlock();

    // 尝试执行
    State expected = State::READY;

    if(m_state.compare_exchange_strong(expected, BUSY))
    {   // 当前状态是ready,变为busy
        solveTasks();
        return true;
    }   
    expected = INIT;
    if(m_state.compare_exchange_strong(expected, BUSY))
    {   // 当前状态是ready,变为busy
        solveTasks();
        return true;
    }   
    
    // 无法执行
    if(m_state == BUSY)
    {   // 直接退出
        return false;
    }
    while(m_state == LOCK)
    {   // 轮询等待
        State curr_task = State::READY;
        if(m_state.compare_exchange_strong(curr_task, BUSY))
        {   // 当前状态是ready,变为busy
            solveTasks();
            return true;
        }   
        curr_task = INIT;
        if(m_state.compare_exchange_strong(curr_task, BUSY))
        {   // 当前状态是ready,变为busy
            solveTasks();
            return true;
        }   
        std::this_thread::yield();
    }
    
    if(m_state == ERROR)
    {
        throw "[fdContext.cc]Error State";
    }
    return false;
}

void FdContextManager::trigger(FdContext::Event event)
{
    RegistedTask::ptr __task = std::make_shared<TRIGGER_TASK>(shared_from_this(), event);
    addTask(__task);
}

void FdContextManager::addEvent(FdContext::Event event, TaskCoro20&& task)
{
    RegistedTask::ptr __task = std::make_shared<ADD_TASK>(shared_from_this(), std::move(task), event);
    addTask(__task);
}

void FdContextManager::delEvent(FdContext::Event event)
{
    RegistedTask::ptr __task = std::make_shared<DEL_TASK>(shared_from_this(), event);
    addTask(__task);
}

void FdContextManager::solveTasks()
{
    while(solveSingleTask()){}
    // 执行完毕，lock后检查是否还有任务
    m_state = LOCK;
    while(solveSingleTask()){}
    // 完全执行完毕，状态返回，退出。
    
    if(!m_fdcontex->hasEvent()) {m_state = INIT; }
    else{m_state = READY;}
}

bool FdContextManager::solveSingleTask()
{

    std::unique_lock<std::shared_mutex> wlock(m_task_mutex);
    std::list<std::shared_ptr<RegistedTask>> tasks;
    tasks.splice(tasks.end(), m_tasks);
    wlock.unlock();

    bool rt = !!tasks.size();
    for(auto& it : tasks)
    {
        try
        {
            it->run();
        }
        catch(std::exception& e)
        {
            M_SYLAR_LOG_ERROR(g_logger) << "failed to run task, error:\n" << e.what();
            throw ;
        }
    }

    return rt;
}




// RegistedTask
using FCM = FdContextManager;
FCM::DEL_TASK::DEL_TASK(FdContextManager::ptr fd_ctx_manager, FdContext::Event event)
    : FCM::RegistedTask(fd_ctx_manager), m_event(event)
{}

void FCM::DEL_TASK::run()
{
    m_fd_ctx_manager->m_fdcontex->delEvent(m_event);
}


FCM::ADD_TASK::ADD_TASK(FdContextManager::ptr fd_ctx_manager, TaskCoro20&& task, FdContext::Event event)
    : FCM::RegistedTask(fd_ctx_manager), m_event(event), m_task(std::move(task))
{}

void FCM::ADD_TASK::run()
{
    m_fd_ctx_manager->m_fdcontex->addEvent(m_event, std::move(m_task));
}



FCM::TRIGGER_TASK::TRIGGER_TASK(FdContextManager::ptr fd_ctx_manager, FdContext::Event event)
    : FCM::RegistedTask(fd_ctx_manager), m_event(event)
{}

void FCM::TRIGGER_TASK::run()
{
    m_fd_ctx_manager->m_fdcontex->trigger(m_event);
}


};
