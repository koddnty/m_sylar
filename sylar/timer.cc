#include "timer.h"
#include <mutex>
#include <shared_mutex>

m_sylar::Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

namespace m_sylar
{

bool Timer::Compare::operator()(const Timer::ptr& lhs, const Timer::ptr& rhs) const
{
    if(!lhs && !rhs)
    {
        return false;
    }
    if(!lhs)
    {
        return true;
    }
    if(!rhs)
    {
        return false;
    }
    if(lhs->m_nextTime < rhs->m_nextTime)
    {
        return true;
    }
    if(lhs->m_nextTime > rhs->m_nextTime)
    {
        return false;
    }
    return &lhs < &rhs;
}

Timer::Timer(uint64_t ms, std::function<void()> cb_func,
            bool recurring, TimeManager* time_manager)
            : m_recurring(recurring),
              m_cycleMs(ms),
              m_cb(cb_func),
              m_manager(time_manager)
{}


Timer::ptr TimeManager::addTimer(uint64_t ms, std::function<void()> cb_func,
                            bool recurring )
{
    Timer::ptr new_timer(new Timer(ms, cb_func, recurring, this));
    
    std::unique_lock<std::shared_mutex> w_lock (m_rwMutex);
    auto it = m_times.insert(new_timer);
    bool is_at_front = (it.first == m_times.begin());
    w_lock.unlock();

    if(is_at_front)
    {
        earlierTimerInserted();
    }
    return new_timer;
}

Timer::ptr TimeManager::addConditionTimer(uint64_t ms, std::function<void()> cb_func,
                                std::weak_ptr<void> weak_cond,
                                bool recurring )
{
    Timer::ptr new_timer(new Timer(ms, cb_func, recurring, this));
    std::unique_lock<std::shared_mutex> w_lock (m_rwMutex);
    auto it = m_times.insert(new_timer);
    bool is_at_front = (it.first == m_times.begin());

    if(is_at_front)
    {
        earlierTimerInserted();
    }
    return new_timer;
}
}