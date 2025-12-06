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

static m_sylar::Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

namespace m_sylar
{
static thread_local TimeManager* t_tim = nullptr;

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
        TimeManager* manager,
        std::function<void()> main_cb,
        std::function<bool()> condition,
        std::function<void()> condition_cb)
      : m_is_cycle(is_cycle), m_interval(intervalTime),
        m_manager(manager), m_main_cb(main_cb)
{
    if(condition == nullptr)
    {
        m_condition = default_condition;
    }
    else 
    {
        m_condition = condition;
    }
    if(condition_cb == nullptr)
    {
        m_condition_cb = default_condition_cb;
    }
    else
    {
        m_condition_cb = condition_cb;
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

    timerfd_settime(m_timeFd, 0, &new_value, NULL);

    // 添加任务
    m_manager->m_iom->addEvent(m_timeFd, IOManager::Event::READ, std::bind(&Timer::runner, shared_from_this())); 
}


void Timer::runner()
{
    // 取出数据
    uint64_t expirations = 0;
    read(m_timeFd, &expirations, sizeof(expirations));
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
    else
    {
        re_enroll();
    }
}

void Timer::re_enroll()
{
    m_manager->m_iom->addEvent(m_timeFd, IOManager::Event::READ, std::bind(&Timer::runner, shared_from_this())); 
}


TimeManager::TimeManager(IOManager::ptr iom)
{
    M_SYLAR_ASSERT(iom);
    m_iom = iom.get();
    t_tim = this;
}

TimeManager::TimeManager(IOManager* iom)
{
    M_SYLAR_ASSERT(iom);
    m_iom = iom;
    t_tim = this;
}

TimeManager::~TimeManager()
{

}


int TimeManager::addTimer(uint64_t intervalTime, bool is_cycle, 
        std::function<void()> main_cb)
{
    Timer::ptr new_timer (new Timer(intervalTime, is_cycle, this, main_cb));

    std::unique_lock<std::shared_mutex> w_lock (m_rwMutex);
    // M_SYLAR_LOG_DEBUG(g_logger) << "add in timerFd map : timerFd=" <<  new_timer->m_timeFd;
    new_timer->enrollToManager();
    m_timersMap[new_timer->m_timeFd] = new_timer;
    return new_timer->m_timeFd;
}               


int TimeManager::addConditionTimer(uint64_t intervalTime, bool is_cycle, 
    std::function<void()> main_cb,
    std::function<bool()> condition,
    std::function<void()> condition_cb)
{
    Timer::ptr new_timer (new Timer(intervalTime, is_cycle, this
                                        , main_cb, condition, condition_cb));

    std::unique_lock<std::shared_mutex> w_lock (m_rwMutex);
    // M_SYLAR_LOG_DEBUG(g_logger) << "add in timerFd map : timerFd=" <<  new_timer->m_timeFd;
    new_timer->enrollToManager();
    m_timersMap[new_timer->m_timeFd] = new_timer;
    return new_timer->m_timeFd;
}


void TimeManager::cancelTimer(Timer::ptr timer)
{
    m_iom->cancelAll(timer->m_timeFd);

    std::unique_lock<std::shared_mutex> w_lock(m_rwMutex);
    
    auto it = m_timersMap.find(timer->m_timeFd);
    if(it != m_timersMap.end())
    {
        m_timersMap.erase(it);
    }
    else
    {
        M_SYLAR_LOG_ERROR(g_logger) << "timer is not in time manager" 
            << "timerFd=" << timer->m_timeFd;
    }
}

void TimeManager::cancelTimer(int timerFd)
{
    m_iom->cancelAll(timerFd);

    std::unique_lock<std::shared_mutex> w_lock(m_rwMutex);
    
    auto it = m_timersMap.find(timerFd);
    if(it != m_timersMap.end())
    {
        m_timersMap.erase(it);
    }
    else
    {
        M_SYLAR_LOG_ERROR(g_logger) << "timer is not in time manager" 
            << "timerFd=" << timerFd;
    }
}

TimeManager* TimeManager::getThis()
{
    if(t_tim)
    {
        return t_tim;
    }
    m_sylar::IOManager* iom = m_sylar::IOManager::getThis();
    if(iom)
    {
        return new TimeManager(iom);
    }
    M_SYLAR_ASSERT2(false, "cant get time manager without a iomanager");
}  



}