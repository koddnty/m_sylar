#pragma once
#include <map>

#include "coroutine/coro20/ioManager.h"
#include "timer_task.hpp"
#include "timer_block.hpp"


/**
    毫秒级定时器，基于Linux的timerfd实现，使用IOManager进行事件注册和触发。提供普通定时器和条件定时器两种类型，支持循环执行和单次执行。
    addEventWithTimeout函数为IO事件添加时间限制功能，方便超时系统开发。
*/
namespace m_sylar
{


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



class TimeManager : public std::enable_shared_from_this<TimeManager> {
public:
    using ptr = std::shared_ptr<TimeManager>;

    TimeManager(IOManager::ptr iom, size_t blockLength);
    TimeManager(IOManager* iom, size_t blockLength);
    ~TimeManager();

    int init();
    int close();


    void onTimerTriggered();     // 定时器触发时的处理函数

    TimeTask::ptr addTimer(TimeTask::ptr time_task);                                                    // 添加普通定时器
    TimeTask::ptr addConditionTimer(TimeTask::ptr time_task);                                           // 添加条件定时器, usec

    IOManager& addEventWithTimeout(int fd, FdContext::Event event, TaskCoro20&& task, 
                                            uint64_t timeout, std::shared_ptr<TimeLimitInfo::State> rtState, int closeFlag = 0);        // usec, 1000,000
    IOManager& addEventWithTimeout(int fd, FdContext::Event event, std::function<void()> cb_func, 
                                            uint64_t timeout, std::shared_ptr<TimeLimitInfo::State> rtState, int closeFlag = 0);



    void cancelTimer(TimeTask::ptr time_task);                             // 取消定时器

    inline size_t getTimerCount() const { return m_timerCount.load(); }     // 获取定时器数量

    // static void setInstance(std::shared_ptr<TimeManager> tim);
    static std::shared_ptr<TimeManager> getInstance();
    static uint64_t GetCurrentMS();                                     // 获取当前时间

    Task<void, TaskBeginExecuter> runTimeTask(TimeTask::ptr timetask);     // 协程睡眠，单位ms
    

    void printInfo();

private:



private:
    TimerBlock::ptr getOrCreateTimerBlock(uint64_t execute_time);                   // 获取或创建时间片
    int insertTimeTask(TimeTask::ptr time_task);                                       // 插入定时器任务到时间片中

    int updateTimerFd(uint64_t execute_time);                             // 更新timerfd的触发时间

private:
    std::atomic<uint64_t> m_nextTimerTime {0xFFFFFFFFFFFFFFFF};                                                   // 下一个定时器触发时间，单位ms
    int m_timerFd = -1;
    std::shared_mutex m_timerFd_mutex;     // 保护m_timerFd的互斥锁
    std::shared_ptr<IOManager> m_iom;                                                   // IOManager指针，定时器通过IOManager实现事件注册和触发
    std::multimap<uint64_t, TimerBlock::ptr> m_time_blocks;             // 定时器任务列表，存储所有定时器任务的迭代器，方便取消定时器{开始时间, TimerBlock}
    std::shared_mutex m_mutex;      
    uint64_t m_block_size_ms {10000};                                      // 时间片大小，单位ms
    uint64_t m_base_time {0};                                                   // 基准时间，单位ms

    std::atomic<size_t> m_timerCount = 0;                       // 定时器数量

};

};


