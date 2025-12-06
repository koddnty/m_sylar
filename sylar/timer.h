#pragma once
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <sys/types.h>
#include "allHeader.h"
#include "log.h"
#include "ioManager.h"


namespace m_sylar_o 
{
    
class TimeManager;


class Timer : public std::enable_shared_from_this<Timer>
{
friend TimeManager;
public:
    using ptr = std::shared_ptr<Timer>;

private:
    Timer(uint64_t ms, std::function<void()> cb_func,
            bool recurring, TimeManager* time_manager);

    struct Compare
    {
        bool operator()(const Timer::ptr& lhs, const Timer::ptr& rhs) const ;
    };

private:
    bool m_recurring = false;               // 是否循环  (否)
    bool m_cycleMs;                         // 循环时间 
    bool m_nextTime;                        // 下一次执行时间
    std::function<void()> m_cb;             // 定时器回调
    TimeManager* m_manager = nullptr;   
};


class TimeManager
{
friend Timer;
public:
    using ptr = std::shared_ptr<TimeManager>;
    Timer::ptr addTimer(uint64_t ms, std::function<void()> cb_func,
                             bool recurring = false);

    Timer::ptr addConditionTimer(uint64_t ms, std::function<void()> cb_func,
                                    std::weak_ptr<void> weak_cond,
                                    bool recurring = false);

protected:
    virtual void earlierTimerInserted() = 0;

private:
    std::shared_mutex m_rwMutex;           
    std::set<Timer::ptr, Timer::Compare> m_times;       // 定时器合集
};

}
