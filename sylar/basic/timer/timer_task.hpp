#pragma once
#include <iostream>
#include <functional>
#include <memory>
#include "coroutine/corobase.h"



namespace m_sylar{

class TimeTask {
public:
    using ptr = std::shared_ptr<TimeTask>;
    TimeTask(std::function<Task<bool>()> main_cb,
             std::function<Task<bool>()> condition,
             std::function<Task<bool>()> condition_cb, bool is_cycle = false)
        : m_main_cb(main_cb), m_condition(condition), m_condition_cb(condition_cb), m_is_cycle(is_cycle) {
        }

    static TimeTask::ptr create(uint64_t ms, bool is_cycle,
        std::function<Task<bool>()> main_cb,
        std::function<Task<bool>()> condition = nullptr,
        std::function<Task<bool>()> condition_cb = nullptr) {
        auto time_task = std::make_shared<TimeTask>(main_cb, condition, condition_cb, is_cycle);
        time_task->m_execute_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count() + ms;
        time_task->m_intreval_ms = ms;
        return time_task;
    }

    ~TimeTask() {
    }


    std::function<Task<bool>()> m_main_cb;            // 主任务函数
    std::function<Task<bool>()> m_condition {nullptr};          // 执行条件,若返回true后才执行main_cb, 否则执行condition_cb
    std::function<Task<bool>()> m_condition_cb {nullptr};       // 条件不成立后执行函数


    inline bool shouldReEnroll() const { return !m_is_canceled && m_is_cycle; }     // 判断是否需要重新加入时间片
    inline bool getIsCycle() const { return m_is_cycle; }
    inline bool getIsCanceled() const { return m_is_canceled; }
    inline uint64_t getExecuteTime() const { return m_execute_ms; }

    Task<bool> runner();
    inline void cancel() { m_is_canceled = true; }     // 取消定时器

private:
    uint64_t next();                                // 更新当前任务下一个执行时间点，并返回下一个执行时间点

    uint64_t m_intreval_ms;                         // 定时间隔，单位ms
    uint64_t m_execute_ms;                          // 任务执行时间点
    bool m_is_cycle {false};                        // 是否循环执行
    std::atomic<bool> m_is_canceled {false};        // 任务是否有效，取消任务后置为false
};



}