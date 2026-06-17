#include <map>

#include "coroutine/coro20/ioManager.h"

/**
    毫秒级定时器，基于Linux的timerfd实现，使用IOManager进行事件注册和触发。提供普通定时器和条件定时器两种类型，支持循环执行和单次执行。通过addEventWithTimeout函数为IO事件添加时间限制功能，方便超时系统开发。
*/
namespace m_sylar
{
class TimeTask {
public:
    using ptr = std::shared_ptr<TimeTask>;

    TimeTask(std::function<Task<bool>()> main_cb,
             std::function<Task<bool>()> condition,
             std::function<Task<bool>()> condition_cb, bool is_cycle = false)
        : m_main_cb(main_cb), m_condition(condition), m_condition_cb(condition_cb), m_is_cycle(is_cycle) {
            std::cout << "----||_TimeTask created, address: "  << this << std::endl << std::flush; ;
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
        std::cout << "----||_TimeTask destroyed, address: "  << this << std::endl << std::flush; ;
    }


    std::function<Task<bool>()> m_main_cb;            // 主任务函数
    std::function<Task<bool>()> m_condition {nullptr};          // 执行条件,若返回true后才执行main_cb, 否则执行condition_cb
    std::function<Task<bool>()> m_condition_cb {nullptr};       // 条件不成立后执行函数

    inline void cancel() { std::cout << "cancelTimer" << std::endl << std::flush; m_is_canceled = true; }     // 取消定时器

    inline bool shouldReEnroll() const { return !m_is_canceled && m_is_cycle; }     // 判断是否需要重新加入时间片
    inline bool getIsCycle() const { return m_is_cycle; }
    inline bool getIsCanceled() const { return m_is_canceled; }
    inline uint64_t getExecuteTime() const { return m_execute_ms; }

    Task<bool> runner();

private:
    uint64_t next();            // 更新当前任务下一个执行时间点，并返回下一个执行时间点

    uint64_t m_intreval_ms;                         // 定时间隔，单位ms
    uint64_t m_execute_ms;                          // 任务执行时间点
    bool m_is_cycle {false};                        // 是否循环执行
    bool m_is_canceled {false};                          // 任务是否有效，取消任务后置为false
};


class n_TimeManager{
public:
    using ptr = std::shared_ptr<n_TimeManager>;

    n_TimeManager(IOManager::ptr iom, size_t blockLength);
    n_TimeManager(IOManager* iom, size_t blockLength);
    ~n_TimeManager();

    int init();
    int close();


    void onTimerTriggered();     // 定时器触发时的处理函数

    TimeTask::ptr addTimer(TimeTask::ptr time_task);                                           // 添加普通定时器
    TimeTask::ptr addConditionTimer(TimeTask::ptr time_task);                                           // 添加条件定时器, usec

    void cancelTimer(TimeTask::ptr time_task);                             // 取消定时器

    inline size_t getTimerCount() const { return m_timerCount.load(); }     // 获取定时器数量

    static n_TimeManager* getInstance();
    static uint64_t GetCurrentMS();                                     // 获取当前时间

    static Task<void, TaskBeginExecuter> runTimeTask(TimeTask::ptr timetask);     // 协程睡眠，单位ms

private:
    class  TimerBlock {
        public:
        using ptr = std::shared_ptr<TimerBlock>;
        int insert(TimeTask::ptr time_task);                    // 插入定时器任务到时间片中
        std::list<TimeTask::ptr> getAndRemove(uint64_t timeMs);                                      // 获取并删除过期的定时器任务

        inline size_t getTaskCount() const { return task_count; }     // 获取时间片内的定时器任务数量
        inline uint64_t getStartTime() const { return start_time; }          // 获取时间片开始时间
        inline uint64_t getEndTime() const { return end_time; }              // 获取

        uint64_t start_time;                                    // 时间片开始时间, ms
        uint64_t end_time;                                      // 时间片结束时间, ms
        std::multimap<uint64_t, TimeTask::ptr> time_tasks;      // 时间片内的定时器任务列表
        std::shared_mutex mutex;                                // 保护time_tasks的互斥锁
        size_t task_count {0};                                    // 时间片内的定时器任务数量
    };


private:
    TimerBlock::ptr getOrCreateTimerBlock(uint64_t execute_time);                   // 获取或创建时间片
    int insertTimeTask(TimeTask::ptr time_task);                                       // 插入定时器任务到时间片中

    int updateTimerFd(uint64_t execute_time);                             // 更新timerfd的触发时间

private:

    int m_timerFd = -1;
    std::shared_mutex m_timerFd_mutex;     // 保护m_timerFd的互斥锁
    IOManager* m_iom;                                                   // IOManager指针，定时器通过IOManager实现事件注册和触发
    std::multimap<uint64_t, TimerBlock::ptr> m_time_blocks;             // 定时器任务列表，存储所有定时器任务的迭代器，方便取消定时器{开始时间, TimerBlock}
    std::shared_mutex m_mutex;      

    uint64_t m_block_size_ms {10000};                                      // 时间片大小，单位ms
    uint64_t m_base_time {0};                                                   // 基准时间，单位ms

    std::atomic<uint64_t> m_nextTimerTime {0xFFFFFFFFFFFFFFFF};                                                   // 下一个定时器触发时间，单位ms
    std::atomic<size_t> m_timerCount = 0;                       // 定时器数量

};

};


