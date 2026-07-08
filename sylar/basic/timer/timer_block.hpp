#pragma once

#include "coroutine/coro20/task.hpp"

#include "timer_task.hpp"


namespace m_sylar {

class  TimerBlock {
public:
    TimerBlock(uint64_t start, uint64_t end);
    TimerBlock(const TimerBlock&) = delete;
    TimerBlock(TimerBlock&&) = delete;

    TimerBlock& operator=(const TimerBlock&) = delete;
    TimerBlock& operator=(TimerBlock&&) = delete;

    using ptr = std::shared_ptr<TimerBlock>;
    int insert(TimeTask::ptr time_task);                    // 插入定时器任务到时间片中
    std::list<TimeTask::ptr> getAndRemove(uint64_t timeMs);                                      // 获取并删除过期的定时器任务

    inline size_t getTaskCount() const { return task_count; }     // 获取时间片内的定时器任务数量
    inline uint64_t getStartTime() const { return start_time; }          // 获取时间片开始时间
    inline uint64_t getEndTime() const { return end_time; }              // 获取
    // inline std::multimap<uint64_t, TimeTask::ptr>& getTimeTasks() { return time_tasks; }    // 获取时间片内的定时器任务列表

    bool empty() const {
        std::shared_lock<std::shared_mutex> rlock(mutex);
        return time_tasks.empty();
    }

    uint64_t getNextExecuteTime() const {
        std::shared_lock<std::shared_mutex> rlock(mutex);
        if(time_tasks.empty()) {
            return UINT64_MAX;
        }
        return time_tasks.begin()->first;
    }

private:
    const uint64_t start_time;                                    // 时间片开始时间, ms
    const uint64_t end_time;                                      // 时间片结束时间, ms
    std::multimap<uint64_t, TimeTask::ptr> time_tasks;      // 时间片内的定时器任务列表
    mutable std::shared_mutex mutex;                                // 保护time_tasks的互斥锁
    std::atomic<size_t> task_count {0};                                    // 时间片内的定时器任务数量
};


}
