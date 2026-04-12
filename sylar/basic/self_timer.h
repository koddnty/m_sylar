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
#include "singleton.h"
#include "coroutine/corobase.h"

#ifdef SYLAR_USE_CORO20
    #include "coroutine/coro20/ioManager.h"
#else
    #include "coroutine/ucontext/ioManager.h"
#endif

namespace m_sylar {

class TimeManager;

  
    



// 定时器
class Timer : public std::enable_shared_from_this<Timer>
{
friend TimeManager;
public:
    using ptr = std::shared_ptr<Timer>;

    Timer(uint64_t intervalTime, bool is_cycle, 
            TimeManager* manager,
            std::function<void()> m_main_cb,
            std::function<bool()> condition= nullptr,
            std::function<void()> condition_cb= nullptr);
    ~Timer();

    struct Compare
    {
        bool operator()(const Timer::ptr& lhs, const Timer::ptr& rhs) const;
    };

protected:

    void enrollToManager();        // 添加到iomanager

    void runner();          // ioManager中imefd注册回调，

private:
// 默认方法
    static bool default_condition() { return true; }
    
    static void default_condition_cb() { return; }

    void re_enroll();

private:
    int m_timeFd;                               // timefd
    bool m_is_cycle;                            // 是否循环
    uint64_t m_interval;                        // 循环间隔时间
    uint64_t m_nextTime;                        // 到期时间
    TimeManager* m_manager;                     // timer管理者
    // std::shared_mutex m_mutex;
    std::function<void()> m_main_cb;            // 主任务函数
    std::function<bool()> m_condition;          // 执行条件（默认无限循环
    std::function<void()> m_condition_cb;       // 条件不成立后执行函数
};




class TimeLimitInfo {
public: 
    enum State {        //  WAITING->FINISHED/ERROR/TIMEOUT
        WAITING = 0,
        FINISHED = 1,
        ERROR = 2,
        TIMEOUT = 3,
    };
    using ptr = std::shared_ptr<TimeLimitInfo>;
    using StatePtr = std::shared_ptr<TimeLimitInfo::State>;
    TimeLimitInfo() {}
    State getState() {
        return m_state.load();
    }
    /** @brief 将原本是origin_state的状态替换为new_state, 失败则返回-1， 成功返回0 */
    int setState(State origin_state, State new_state) {
        State expect = origin_state;
        if(m_state.compare_exchange_strong(expect, new_state)) {
            return 0;
        }
        else {
            return -1;
        }
    }
protected:
    std::atomic<State> m_state = WAITING;
};



// 定时器管理类
class TimeManager
{
friend Timer;
public:
    using ptr = std::shared_ptr<TimeManager>;

    TimeManager(IOManager::ptr iom);
    TimeManager(IOManager* iom);
    ~TimeManager();

    int addTimer(uint64_t intervalTime, bool is_cycle, 
        std::function<void()> main_cb);                                // 添加普通定时器
    
    int addConditionTimer(uint64_t intervalTime, bool is_cycle, 
        std::function<void()> main_cb,
        std::function<bool()> condition,
        std::function<void()> condition_cb);                        // 添加条件定时器, usec

    /** 
        @brief _WithTimeout系列函数，在原有addEvent基础上为事件添加i时间限制功能，方便诸如超时系统开发。

        @param closeFlag 此参数正常情况下不要修改，如果改为非0值，将会在一个事件超时后直接关闭fd管理（以及所有事件）， 详见closeWithNoClose.  
    */

    IOManager& addEventWithTimeout(int fd, FdContext::Event event, TaskCoro20&& task, 
                                            uint64_t timeout, std::shared_ptr<TimeLimitInfo::State> rtState, int closeFlag = 0);        // usec, 1000,000
    IOManager& addEventWithTimeout(int fd, FdContext::Event event, std::function<void()> cb_func, 
                                            uint64_t timeout, std::shared_ptr<TimeLimitInfo::State> rtState, int closeFlag = 0);


    void cancelTimer(Timer::ptr timer);                             // 取消定时器
    void cancelTimer(int timerFd);

    static TimeManager* getInstance();         // 获取timeManager

private:
    std::shared_mutex m_mutex;
    // std::set<Timer::ptr, Timer::Compare> m_timers;                  // 定时器合集
    std::map<int, Timer::ptr> m_timersMap;                      // （Timer唯一存储点）将决定timer的生命周期     
    IOManager* m_iom;                                           // 定时器管理类

};


}