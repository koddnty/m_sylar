#pragma once
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <sys/eventfd.h>
#include <sys/types.h>
#include <utility>
#include "allHeader.h"
#include "log.h"
#include "ioManager.h"
#include "timer.h"

namespace m_sylar {

class TimeManager;

  
    



// 定时器
class Timer : public std::enable_shared_from_this<Timer>
{
friend TimeManager;
public:
    using ptr = std::shared_ptr<Timer>;

    Timer(uint64_t intervalTime, bool is_cycle, 
            std::shared_ptr<TimeManager> manager,
            std::function<void()> m_main_cb,
            std::function<bool()> condition= nullptr,
            std::function<void()> condition_cb= nullptr);
    ~Timer();

    struct Compare
    {
        bool operator()(const Timer::ptr& lhs, const Timer::ptr& rhs) const;
    };

    void enrollToManager();        // 添加到iomanager

    void runner();          // ioManager中imefd注册回调，

private:
// 默认方法
    static bool default_condition() { return true; }
    
    static void default_condition_cb() { return; }

private:
    int m_timeFd;                               // timefd
    bool m_is_cycle;                            // 是否循环
    uint64_t m_interval;                        // 循环间隔时间
    uint64_t m_nextTime;                        // 到期时间
    std::shared_ptr<TimeManager> m_manager;     // timer管理者
    std::function<void()> m_main_cb;            // 主任务函数
    std::function<bool()> m_condition;          // 执行条件（默认无限循环
    std::function<void()> m_condition_cb;       // 条件不成立后执行函数
};






// 定时器管理类
class TimeManager : public std::enable_shared_from_this<TimeManager>
{
friend Timer;
public:
    using ptr = std::shared_ptr<TimeManager>;

    TimeManager(IOManager::ptr iom);
    ~TimeManager();

    void addTimer(uint64_t intervalTime, bool is_cycle, 
        std::shared_ptr<TimeManager> manager,
        std::function<void()> main_cb);                                // 添加普通定时器
    
    void addConditionTimer(uint64_t intervalTime, bool is_cycle, 
        std::shared_ptr<TimeManager> manager,
        std::function<void()> main_cb,
        std::function<bool()> condition,
        std::function<void()> condition_cb);                        // 添加条件定时器

    void cancelTimer(Timer::ptr timer);                             // 取消定时器

private:
    std::shared_mutex m_rwMutex;
    std::set<Timer::ptr, Timer::Compare> m_timers;                  // 定时器合集
    IOManager::ptr m_iom;                                           // 定时器管理类
};

}