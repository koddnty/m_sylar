#include "fdContext.h"
#include "hook.h"
#include <string.h>

namespace m_sylar
{

static Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");


// FdContext
FdContext::FdContext(int fd)
{
    m_fd = fd;
}

FdContext::~FdContext()
{}

FdContext& FdContext::addEvent(Event event, TaskCoro20&& task)
{
    m_event = Event(m_event | event);
    if(event & READ)
    {
        m_cb_read = std::move(task);
    }
    if(event & WRITE)
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
        if(t.isLegal())
        {
            t.start();
            // M_SYLAR_LOG_ERROR(g_logger) << "task(TaskCoro20) is illegal";
        }
    }
    if(merge & WRITE)
    {
        auto t = m_cb_write.clone();
        if(t.isLegal())
        {
            t.start();
        }
    }
    return *this;
}





FdContext& FdContext::reset()
{
    m_cb_read.reset();
    m_cb_write.reset();
    m_event = NONE;
    return *this;
}   
    


// FdContextManager
FdContextManager::FdContextManager(int fd)
{
    m_fdcontex = std::make_shared<FdContext>(fd);
    m_fdcontex->reset();
}

FdContextManager::~FdContextManager()
{}

bool FdContextManager::addTask(std::shared_ptr<RegistedTask> task)
{
    // M_SYLAR_ASSERT2(m_state == INIT, "")
    // 事件注册
    std::unique_lock<std::shared_mutex> wlock(m_task_mutex);
    if(task != nullptr) 
    {
        m_tasks.push_back(task);
    }
    if(m_tasks.empty()) {return true;}
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
    
    if(m_state == ERROR)
    {
        throw "[fdContext.cc]Error State";
    }
    return false;
}

void FdContextManager::trigger(FdContext::Event event, std::function<void(FdContext::ptr, FdContext::Event )> cb)
{
    RegistedTask::ptr __task = std::make_shared<TRIGGER_TASK>(shared_from_this(), event, cb);
    addTask(__task);
}

void FdContextManager::addEvent(FdContext::Event event, TaskCoro20&& task, std::function<void(FdContext::ptr, FdContext::Event)> cb)
{
    RegistedTask::ptr __task = std::make_shared<ADD_TASK>(shared_from_this(), std::move(task), event, cb);
    addTask(__task);
}

void FdContextManager::delEvent(FdContext::Event event, std::function<void(FdContext::ptr, FdContext::Event )> cb)
{
    RegistedTask::ptr __task = std::make_shared<DEL_TASK>(shared_from_this(), event, cb);
    addTask(__task);
}

void FdContextManager::closeEvent(FdContext::Event event, std::function<void(FdContext::ptr, FdContext::Event )> cb)
{
    RegistedTask::ptr __task = std::make_shared<CLOSE_TASK>(shared_from_this(), event, cb);
    addTask(__task);
}

void FdContextManager::closeEventNoCloseFd(FdContext::Event event, std::function<void(FdContext::ptr, FdContext::Event )> cb)
{
    RegistedTask::ptr __task = std::make_shared<CLOSE_TASK_NOCLOSEFD>(shared_from_this(), event, cb);
    addTask(__task);
}


void FdContextManager::solveTasks()
{   // 函数调用时状态为busy
    
    while(solveSingleTask()){}
    // 执行完毕，lock后检查是否还有任务
    // m_state = LOCK;
    while(solveSingleTask()){}
    // 完全执行完毕，状态返回，退出。
    
    if(!m_fdcontex->hasEvent()) {m_state = INIT; }
    else{m_state = READY;}
    addTask(nullptr);
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
FCM::RegistedTask::RegistedTask(std::shared_ptr<FdContextManager> fd_ctx_manager, std::function<void(FdContext::ptr, FdContext::Event )> m_cb)
    : m_fd_ctx_manager(fd_ctx_manager), m_cb(m_cb)
{}

FCM::RegistedTask::~RegistedTask()
{}

FCM::DEL_TASK::DEL_TASK(FdContextManager::ptr fd_ctx_manager, FdContext::Event event, std::function<void(FdContext::ptr, FdContext::Event)> m_cb)
    : FCM::RegistedTask(fd_ctx_manager, m_cb), m_event(event)
{}

void FCM::DEL_TASK::run()
{
    // std::cout << "delete task, fd=" << m_fd_ctx_manager->m_fdcontex->getFd() << std::endl;
    FdContext::Event  origin_state = m_fd_ctx_manager->m_fdcontex->getEvent();
    m_fd_ctx_manager->m_fdcontex->delEvent(m_event);
    if(m_cb)
        m_cb(m_fd_ctx_manager->m_fdcontex, origin_state); // 先状态同步

}


FCM::ADD_TASK::ADD_TASK(FdContextManager::ptr fd_ctx_manager, TaskCoro20&& task, FdContext::Event event, std::function<void(FdContext::ptr, FdContext::Event)> m_cb)
    : FCM::RegistedTask(fd_ctx_manager, m_cb), m_event(event), m_task(std::move(task))
{}

void FCM::ADD_TASK::run()
{
    // std::cout << "add task, fd=" << m_fd_ctx_manager->m_fdcontex->getFd() << std::endl;
    FdContext::Event  origin_state = m_fd_ctx_manager->m_fdcontex->getEvent();
    m_fd_ctx_manager->m_fdcontex->addEvent(m_event, std::move(m_task));
    if(m_cb)
        m_cb(m_fd_ctx_manager->m_fdcontex, origin_state); // 状态同步
}



FCM::TRIGGER_TASK::TRIGGER_TASK(FdContextManager::ptr fd_ctx_manager, FdContext::Event event, std::function<void(FdContext::ptr, FdContext::Event)> m_cb)
    : FCM::RegistedTask(fd_ctx_manager, m_cb), m_event(event)
{}

void FCM::TRIGGER_TASK::run()
{
    // std::cout << "trigger task, fd=" << m_fd_ctx_manager->m_fdcontex->getFd() << std::endl;
    FdContext::Event  origin_state = m_fd_ctx_manager->m_fdcontex->getEvent();
    m_fd_ctx_manager->m_fdcontex->trigger(m_event);
    if(m_cb)
        m_cb(m_fd_ctx_manager->m_fdcontex, origin_state); // 状态同步
}


FCM::CLOSE_TASK::CLOSE_TASK(FdContextManager::ptr fd_ctx_manager, FdContext::Event event, std::function<void(FdContext::ptr, FdContext::Event)> m_cb)
    : FCM::RegistedTask(fd_ctx_manager, m_cb), m_event(event)
{}

void FCM::CLOSE_TASK::run()
{
    FdContext::Event  origin_state = m_fd_ctx_manager->m_fdcontex->getEvent();
    // m_fd_ctx_manager->m_fdcontex->trigger(m_event);
    // std::cout << "close task, fd=" << m_fd_ctx_manager->m_fdcontex->getFd() << std::endl;
    int fd = m_fd_ctx_manager->m_fdcontex->getFd();
    m_fd_ctx_manager->m_fdcontex->reset();
    if(m_cb)
        m_cb(m_fd_ctx_manager->m_fdcontex, origin_state); // 状态同步
    co_close(fd);
    // m_fd_ctx_manager->m_fdcontex->reset();
}


FCM::CLOSE_TASK_NOCLOSEFD::CLOSE_TASK_NOCLOSEFD(FdContextManager::ptr fd_ctx_manager, FdContext::Event event, std::function<void(FdContext::ptr, FdContext::Event)> m_cb)
    : FCM::RegistedTask(fd_ctx_manager, m_cb), m_event(event)
{}

void FCM::CLOSE_TASK_NOCLOSEFD::run()
{
    FdContext::Event  origin_state = m_fd_ctx_manager->m_fdcontex->getEvent();
    int fd = m_fd_ctx_manager->m_fdcontex->getFd();
    m_fd_ctx_manager->m_fdcontex->reset();
    if(m_cb)
        m_cb(m_fd_ctx_manager->m_fdcontex, origin_state); // 状态同步
    co_close(fd, 1);
}



};
