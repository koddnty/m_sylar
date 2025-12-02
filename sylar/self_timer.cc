#include "self_timer.h"
#include "ioManager.h"
#include "log.h"
#include "macro.h"
#include "scheduler.h"
#include <algorithm>
#include <cstddef>
#include <ctime>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unistd.h>
#include <sys/timerfd.h>
#include <functional>

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
    if(lhs->m_interval < rhs->m_interval)
    {
        return true;
    }
    if(lhs->m_interval > rhs->m_interval)
    {
        return false;
    }
    return lhs->m_timeFd < rhs->m_timeFd;
}

// us级intervalTime
Timer::Timer(uint64_t intervalTime, bool is_cycle, 
        std::shared_ptr<TimeManager> manager,
        std::function<void()> main_cb,
        std::function<bool()> condition,
        std::function<void()> condition_cb)
      : m_interval(intervalTime), m_is_cycle(is_cycle),
        m_main_cb(main_cb), m_manager(manager)
{
    if(condition == nullptr)
    {
        m_condition = default_condition;
    }
    if(condition_cb == nullptr)
    {
        m_condition_cb = default_condition_cb;
    }
}


Timer::~Timer()
{
    close(m_timeFd);  
}


void Timer::enrollToManager()
{
    // 设置定时器
    struct itimerspec new_value;
    new_value.it_value.tv_sec =  m_interval / 1000000;
    new_value.it_value.tv_nsec = (m_interval * 1000) % 1000000000;
    if(m_is_cycle)
    {
        new_value.it_interval.tv_sec = m_interval / 1000000;
        new_value.it_interval.tv_nsec = (m_interval * 1000) % 1000000000;
    }
    else
    {
        // 非循环任务
        new_value.it_interval.tv_sec = 0;
        new_value.it_interval.tv_nsec = 0;
    }

    m_timeFd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);

    int rt = timerfd_settime(m_timeFd, 0, &new_value, NULL);

    // 添加任务
    m_manager->m_iom->addEvent(m_timeFd, IOManager::Event::READ, std::bind(&Timer::runner, shared_from_this())); 
}


void Timer::runner()
{
    // 执行回调
    if(m_condition())
    {
        m_main_cb();
    }
    else
    {
        m_condition_cb();
    }

    // 非循环终止
    if(!m_is_cycle)
    {
        m_manager->cancelTimer(shared_from_this());
    }
}


TimeManager::TimeManager(IOManager::ptr iom)
{
    M_SYLAR_ASSERT(iom);
    m_iom = iom;
}


TimeManager::~TimeManager()
{

}


void TimeManager::addTimer(uint64_t intervalTime, bool is_cycle, 
        std::shared_ptr<TimeManager> manager,
        std::function<void()> main_cb)
{
    Timer::ptr new_timer (new Timer(intervalTime, is_cycle, shared_from_this(), main_cb));

    std::unique_lock<std::shared_mutex> w_lock (m_rwMutex);
    m_timers.insert(new_timer);
    new_timer->enrollToManager();
}               


void TimeManager::addConditionTimer(uint64_t intervalTime, bool is_cycle, 
    std::shared_ptr<TimeManager> manager,
    std::function<void()> main_cb,
    std::function<bool()> condition,
    std::function<void()> condition_cb)
{
    Timer::ptr new_timer (new Timer(intervalTime, is_cycle, shared_from_this()
                                        , main_cb, condition, condition_cb));

    std::unique_lock<std::shared_mutex> w_lock (m_rwMutex);
    m_timers.insert(new_timer);
    new_timer->enrollToManager();
}


void TimeManager::cancelTimer(Timer::ptr timer)
{
    m_iom->delEvent(timer->m_timeFd, IOManager::Event::READ);

    std::unique_lock<std::shared_mutex> w_lock(m_rwMutex);
    auto it = m_timers.erase(timer);

    if(!it)
    {
        M_SYLAR_LOG_ERROR(g_logger) << "timer is not in time manager" 
            << "timerFd=" << timer->m_timeFd;
    }
}


}