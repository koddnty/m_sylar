#include "timer.hpp"
#include <sys/timerfd.h>
#include "basic/config.h"



namespace m_sylar {
static std::shared_ptr<n_TimeManager> t_tim = nullptr;
static m_sylar::Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

ConfigVar<uint32_t>::ptr g_basic_timer = ConfigManager::LookUp("basic.timer.blocklength", uint32_t(3000), 0, "默认单个时间块的时间长度，单位ms,越小锁竞争越少");






n_TimeManager::n_TimeManager(IOManager::ptr iom, size_t blockLength) {
    m_iom = iom.get();
    m_block_size_ms = blockLength;
}

n_TimeManager::n_TimeManager(IOManager* iom, size_t blockLength) {
    m_iom = iom;
    m_block_size_ms = blockLength;
}

n_TimeManager::~n_TimeManager() {
    if(m_timerFd != -1) {
        close();
        m_timerFd = -1;
    }
    return;
}

int n_TimeManager::init() {
    // 设置定时器
    m_timerFd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if(m_timerFd == -1) {
        M_SYLAR_LOG_ERROR(g_logger) << "failed to create timerfd, error code" << (errno) << ": " << strerror(errno);
        return -1;
    }
    m_iom->addEvent(m_timerFd, FdContext::Event::READ, 
        TaskCoro20::create_func([this]() {
            this->onTimerTriggered();   // 定时器触发时的处理
        }
    ));
    m_base_time = GetCurrentMS();
    return 0;
}

int n_TimeManager::close() {
    if(m_timerFd != -1) {
        m_iom->delEvent(m_timerFd, FdContext::Event::READ);
    }
    return 0;
}

void n_TimeManager::setInstance(std::shared_ptr<n_TimeManager> tim) {
    t_tim = tim;
}


n_TimeManager::ptr n_TimeManager::getInstance() {
    if(t_tim) {
        return t_tim;
    }
    m_sylar::IOManager* iom = m_sylar::IOManager::getInstance();
    if(iom) {
        t_tim = std::make_shared<n_TimeManager>(iom, g_basic_timer->getValue());
        t_tim->init();
        return t_tim;
    }
    return nullptr;
}

void n_TimeManager::onTimerTriggered() {
    uint64_t now = GetCurrentMS();              // 加1ms的误差补偿，确保不会漏掉过期的定时器任务
    M_SYLAR_LOG_DEBUG(g_logger) << "timer triggered, now: " << now;
    std::vector<TimerBlock::ptr> expired_tasks;     // 存储过期的定时器任务
    {
        std::unique_lock<std::shared_mutex> rlock(m_mutex);
        auto it = m_time_blocks.begin();
        while(it != m_time_blocks.end() && it->first <= now) {
            M_SYLAR_LOG_DEBUG(g_logger) << "found expired timer block, block_start_time: " << it->second->getStartTime() << ", block_end_time: " << it->second->getEndTime();
            expired_tasks.push_back(it->second);     // 将可能的时间块加入到过期任务列表中
            it++;
        }
    }

    // 取出所有过期任务
    std::list<TimeTask::ptr> expired_time_tasks;     // 存储过期的定时器任务
    for(auto& timer_block : expired_tasks) {
        auto expired_tasks_in_block = timer_block->getAndRemove(now);     // 获取并删除过期的定时器任务
        for(auto& it : expired_tasks_in_block) {
            M_SYLAR_LOG_DEBUG(g_logger) << "found expired timer task, execute_time: " << it->getExecuteTime() << ", is_cycle: " << it->getIsCycle();
        }
        expired_time_tasks.insert(expired_time_tasks.end(), expired_tasks_in_block.begin(), expired_tasks_in_block.end());
    }

    M_SYLAR_LOG_DEBUG(g_logger) << "total expired timer task count: " << expired_time_tasks.size() << ", cleaning expired timer blocks";
    // 清空过期的时间块
    {
        std::unique_lock<std::shared_mutex> wlock(m_mutex);
        auto it = m_time_blocks.begin();
        while(it != m_time_blocks.end() && it->second->end_time <= now) {
            it = m_time_blocks.erase(it);     // 从时间块列表中删除过期的时间块
        }
    }

    M_SYLAR_LOG_DEBUG(g_logger) << "expired timer blocks cleaned, total expired timer task count: " << expired_time_tasks.size() << ", update next timer time";
    // 设置下一个定时器触发时间}
    {
        std::shared_lock<std::shared_mutex> rlock(m_mutex);
        if(m_time_blocks.empty()) {
            m_nextTimerTime = UINT64_MAX;
            rlock.unlock();
        }
        else {
            TimerBlock::ptr block = m_time_blocks.begin()->second;
            if(!block->time_tasks.empty()) {
                m_nextTimerTime = UINT64_MAX;
                auto time = block->time_tasks.begin()->first;
                rlock.unlock(); 
                if(-1 == updateTimerFd(time)) {
                    //M_SYLAR_LOG_ERROR(g_logger) << "failed to update timerfd time, next_timer_time: ";
                }
            }
            else {
                m_nextTimerTime = UINT64_MAX;
            }
        }
    }


    // if(m_time_blocks.empty()) {
    //     m_nextTimerTime = UINT64_MAX;
    //     rlock.unlock();
    // }
    // else {
    //     TimerBlock::ptr block = m_time_blocks.begin()->second;
    //     rlock.unlock();
    //     if(!block->time_tasks.empty()) {
    //         m_nextTimerTime = block->time_tasks.begin()->first;
    //         if(-1 == updateTimerFd(m_nextTimerTime)) {
    //             M_SYLAR_LOG_ERROR(g_logger) << "failed to update timerfd time, next_timer_time: " << m_nextTimerTime;
    //         }
    //     }
    //     else {
    //         m_nextTimerTime = UINT64_MAX;
    //     }
    // }

    M_SYLAR_LOG_DEBUG(g_logger) << "next timer time updated, next_timer_time: " << m_nextTimerTime << ", scheduling expired timer tasks";
    // 调度执行过期的定时器任务
    for(auto& time_task : expired_time_tasks) {
        M_SYLAR_LOG_DEBUG(g_logger) << "scheduling time task, address: " << time_task.get();
        m_timerCount--;
        auto t = std::bind(&runTimeTask, time_task);
        m_iom->schedule(TaskCoro20::create_coro(t));
    }
}

TimeTask::ptr n_TimeManager::addTimer(TimeTask::ptr time_task) {
    // 插入定时器任务到时间片中
    if(insertTimeTask(time_task) != 0) {
        M_SYLAR_LOG_ERROR(g_logger) << "failed to insert time task, execute_time: " << time_task->getExecuteTime();
    }
    return time_task;
}    

TimeTask::ptr n_TimeManager::addConditionTimer(TimeTask::ptr time_task){
    if(time_task->m_condition == nullptr || time_task->m_condition_cb == nullptr) {
        M_SYLAR_LOG_ERROR(g_logger) << "condition timer must have condition function";
        return time_task;
    }
    // 添加条件定时器, usec
    if(insertTimeTask(time_task) != 0) {
        M_SYLAR_LOG_ERROR(g_logger) << "failed to insert condition time task, execute_time: " << time_task->getExecuteTime();
    }
    return time_task;
}

uint64_t n_TimeManager::GetCurrentMS() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
}

void n_TimeManager::cancelTimer(TimeTask::ptr time_task) {
    time_task->cancel();
    
}

Task<void, TaskBeginExecuter> n_TimeManager::runTimeTask(TimeTask::ptr timetask) {
    M_SYLAR_LOG_DEBUG(g_logger) << "execute timer task, is canceled: " << timetask->getIsCanceled() << ", execute_time: " << timetask->getExecuteTime() << ", is_cycle: " << timetask->getIsCycle()
                << " address: " << timetask.get();
    co_await timetask->runner();       // 运行后会自动更新时间戳
    if(!timetask->getIsCanceled() && timetask->getIsCycle()) {
        // 重新插入定时器
        n_TimeManager::getInstance()->insertTimeTask(timetask);
        co_return;
    }
    M_SYLAR_LOG_DEBUG(g_logger) << "timer task finished, execute_time: " << timetask->getExecuteTime() << ", is_cycle: " << timetask->getIsCycle() 
                << " address: " << timetask.get();
    co_return;
}

TimerBlock::ptr n_TimeManager::getOrCreateTimerBlock(uint64_t execute_time) {
    // 检查时间范围合理性
    auto now = GetCurrentMS();
    if(execute_time < now) {
        M_SYLAR_LOG_ERROR(g_logger) << "execute_time is in the past, execute_time: " << execute_time << ", current_time: " << now;
        return nullptr;
    }

    // 范围查询 [lo, hi)
    uint64_t begin = m_base_time + (execute_time - m_base_time) / m_block_size_ms * m_block_size_ms;

    std::shared_lock<std::shared_mutex> rlock(m_mutex);
    auto it = m_time_blocks.find(begin);
    if(it != m_time_blocks.end()) {
        return it->second;
    }
    rlock.unlock();

    // 没有找到合适的时间片，创建新的时间片
    TimerBlock::ptr new_block = std::make_shared<TimerBlock>();
    // 创建时间片
    uint64_t end = begin + m_block_size_ms;
    new_block->start_time = begin;
    new_block->end_time = end;
    M_SYLAR_ASSERT2(begin <= execute_time && execute_time < end, "bad timerBlockRange");     // 确保时间片不存在

    // 插入时间片
    std::unique_lock<std::shared_mutex> wlock(m_mutex);
    it = m_time_blocks.find(begin);
    if(it != m_time_blocks.end()) {
        return it->second;
    }
    m_time_blocks.insert({begin, new_block});
    return new_block;
}


int n_TimeManager::insertTimeTask(TimeTask::ptr time_task) {
    if(!time_task) {
        return -1;
    }
    uint64_t execute_time = time_task->getExecuteTime();

    auto now = GetCurrentMS();
    if(execute_time <= now) {     // 时间已过，直接执行,不插入时间片
        M_SYLAR_LOG_WARN(g_logger) << "execute_time is in the past, execute_time: " << execute_time << ", current_time: " << now;
        auto t = std::bind(&runTimeTask, time_task);
        m_iom->schedule(TaskCoro20::create_coro(t));
        return 0;
    }

    // 获取时间片
    TimerBlock::ptr timer_block = getOrCreateTimerBlock(execute_time);
    if(!timer_block) {
        return -1;
    }

    M_SYLAR_LOG_DEBUG(g_logger) << "inserting time task, execute_time: " << execute_time << ", current_time: " << now
        << ", block_start: " << timer_block->start_time << ", block_end: " << timer_block->end_time;
    // 计算时间限制
    if(execute_time < timer_block->start_time || execute_time >= timer_block->end_time) {       // 时间不合法，超出时间片范围
        M_SYLAR_LOG_ERROR(g_logger) << "execute_time is out of timer block range, execute_time: " << execute_time
            << ", block_start: " << timer_block->start_time << ", block_end: " << timer_block->end_time;
        return -1;
    }

    // 插入时间片
    timer_block->insert(time_task);

    // timerfd时间调整
    M_SYLAR_LOG_DEBUG(g_logger) << "checking if need to update timerfd time, execute_time: " << execute_time << ", current_time: " << now
        << ", next_timer_time: " << m_nextTimerTime.load();
    if(execute_time < m_nextTimerTime.load()) {   // 需更新timerfd的时间
        M_SYLAR_LOG_DEBUG(g_logger) << "updating timerfd time, execute_time: " << execute_time << ", current_time: " << now
            << ", next_timer_time: " << m_nextTimerTime.load();
        if(-1 == updateTimerFd(execute_time) ) {
            M_SYLAR_LOG_ERROR(g_logger) << "failed to update timerfd time, execute_time: " << execute_time; 
            return -1;
        }
    }

    // 更新状态信息
    m_timerCount++;
    return 0;
}

int n_TimeManager::updateTimerFd(uint64_t execute_time) {
    auto now = GetCurrentMS();
    struct itimerspec new_value;
    memset(&new_value, 0, sizeof(new_value));
    if(execute_time <= now) {
        new_value.it_value.tv_sec = 0;
        new_value.it_value.tv_nsec = 1000000;     // 1ms的误差补偿，确保不会漏掉过期的定时器任务
    }
    else {
        new_value.it_value.tv_sec = (execute_time - now) / 1000;
        new_value.it_value.tv_nsec = ((execute_time - now) % 1000) * 1000000;
    }


    std::unique_lock<std::shared_mutex> wlock {m_timerFd_mutex};
    M_SYLAR_LOG_DEBUG(g_logger) << "updating timerfd time, execute_time: " << execute_time << ", current_time: " << now
        << ", next_timer_time: " << m_nextTimerTime.load();
    if(execute_time >= m_nextTimerTime) {
        M_SYLAR_LOG_DEBUG(g_logger) << "no need to update timerfd time, execute_time: " << execute_time << ", next_timer_time: " << m_nextTimerTime.load();
        return 0; 
    }
    if (timerfd_settime(m_timerFd, 0, &new_value, nullptr) == -1) {
        M_SYLAR_LOG_ERROR(g_logger) << "failed to set timerfd(timer fd = " << m_timerFd << ") time, error code: " << errno << ": " << strerror(errno);
        return -1;
    }
    m_nextTimerTime = execute_time;
    return 0;
}   





}