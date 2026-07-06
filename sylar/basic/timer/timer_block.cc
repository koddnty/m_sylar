#include "timer_block.hpp"

namespace m_sylar {
int TimerBlock::insert(TimeTask::ptr time_task) {
    std::unique_lock<std::shared_mutex> wlock(mutex);
    task_count++;
    return time_tasks.insert({time_task->getExecuteTime(), time_task}) != time_tasks.end() ? 0 : -1;
}


std::list<TimeTask::ptr> TimerBlock::getAndRemove(uint64_t timeMs) {
    std::list<TimeTask::ptr> expired_tasks;
    std::unique_lock<std::shared_mutex> wlock(mutex);
    auto it = time_tasks.begin();
    while(it != time_tasks.end() && it->first <= timeMs) {
        expired_tasks.push_back(it->second);
        it++;
        task_count--;
    }
    time_tasks.erase(time_tasks.begin(), it);    // 从时间块中删除过期的定时器任务
    return expired_tasks;
}


}