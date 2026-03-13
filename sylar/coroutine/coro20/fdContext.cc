#include "fdContext.h"

namespace m_sylar
{

static Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

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
    TRIGGER_TASK::ptr task = std::make_shared<TRIGGER_TASK>(shared_from_this(), event);
    addTask(task);
}

void FdContextManager::solveTasks()
{
    while(solveSingleTask()){}
    // 执行完毕，lock后检查是否还有任务
    m_state = LOCK;
    while(solveSingleTask()){}
    // 完全执行完毕，状态返回，退出。
    
    if(!m_fdcontex->isValid()) {m_state = INIT; }
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
            throw e;
        }
    }

    return rt;
}
};