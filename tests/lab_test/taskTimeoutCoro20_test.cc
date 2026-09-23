#include "basic/config.h"
#include "basic/log.h"
#include "basic/timer/timer.hpp"
#include "coroutine/corobase.h"

#include <atomic>
#include <cerrno>
#include <iostream>
#include <memory>
#include <string>
#include <unistd.h>

/**
    TimeManager::addTaskWithTimeout 测试(普通任务限时执行):
        1. 协程任务在限定时间内结束 -> 出参为FINISHED, 超时回调不执行, 且超时时间到达后状态不被改写
        2. 协程任务超过限定时间   -> 出参为TIMEOUT, 超时回调执行一次; 任务随后自然结束也不改写状态
        3. 普通函数任务按时结束   -> 出参为FINISHED
        4. 普通函数任务超时       -> 出参为TIMEOUT, 由超时回调放行后任务仍会执行完(协作式, 不强制终止)
        5. 出参与超时回调均可为nullptr, 不崩溃
*/

using namespace m_sylar;

static Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");
static int g_failed = 0;

static void check(bool ok, const std::string& what)
{
    std::cout << (ok ? "[ PASS ] " : "[ FAIL ] ") << what << std::endl;
    if(!ok) { ++g_failed; }
}

static bool waitValue(std::atomic<int>& v, int init, uint64_t timeout_ms)
{
    uint64_t begin = TimeManager::GetCurrentMS();
    while(v.load() == init)
    {
        if(TimeManager::GetCurrentMS() - begin > timeout_ms) { return false; }
        usleep(1000);
    }
    return true;
}

static const char* stateName(int state)
{
    switch(state)
    {
        case TimeLimitInfo::WAITING:  return "WAITING";
        case TimeLimitInfo::FINISHED: return "FINISHED";
        case TimeLimitInfo::TIMEOUT:  return "TIMEOUT";
        case TimeLimitInfo::ERROR:    return "ERROR";
        default:                      return "UNKNOWN";
    }
}

// ------------------------- 任务定义 -------------------------
struct TaskCase
{
    std::atomic<int> task_done {0};         // 任务体执行完毕
    std::atomic<int> timeout_cb_count {0};  // 超时回调执行次数
    std::atomic<int> task_count {0};        // 任务体被执行次数
};

// 睡眠指定ms的协程任务
static Task<void, TaskBeginExecuter> coroSleepTask(std::shared_ptr<TaskCase> c, uint64_t ms)
{
    c->task_count++;
    co_await co_sleep(ms);
    c->task_done = 1;
    co_return;
}

// 等待release标记的普通函数任务
static std::atomic<int> g_release {0};

// ------------------------- 用例 -------------------------

// 1. 协程任务在限定时间内结束
static void testCoroFinishInTime(IOManager* iom, TimeManager* tim)
{
    auto c = std::make_shared<TaskCase>();
    auto state = std::make_shared<TimeLimitInfo::State>(TimeLimitInfo::WAITING);

    tim->addTaskWithTimeout(
        TaskCoro20::create_coro([c]() { return coroSleepTask(c, 100); }),
        1000, state, [c]() { c->timeout_cb_count++; });

    bool done = waitValue(c->task_done, 0, 3000);
    check(done, "协程任务按时结束: 任务已执行完");
    // 超过超时时间, 确认状态没有被超时分支改写
    usleep(1200 * 1000);
    check(*state == TimeLimitInfo::FINISHED,
          std::string("协程任务按时结束: 出参为FINISHED, 实际=") + stateName(*state));
    check(c->timeout_cb_count == 0,
          "协程任务按时结束: 超时回调未执行, count=" + std::to_string(c->timeout_cb_count.load()));
}

// 2. 协程任务超时
static void testCoroTimeout(IOManager* iom, TimeManager* tim)
{
    auto c = std::make_shared<TaskCase>();
    auto state = std::make_shared<TimeLimitInfo::State>(TimeLimitInfo::WAITING);

    uint64_t begin = TimeManager::GetCurrentMS();
    tim->addTaskWithTimeout(
        TaskCoro20::create_coro([c]() { return coroSleepTask(c, 1200); }),
        200, state, [c]() { c->timeout_cb_count++; });

    bool cb = waitValue(c->timeout_cb_count, 0, 2000);
    uint64_t cost = TimeManager::GetCurrentMS() - begin;
    check(cb, "协程任务超时: 超时回调被执行");
    check(cost >= 150 && cost <= 1000,
          "协程任务超时: 约200ms后触发, 实际=" + std::to_string(cost) + "ms");
    check(*state == TimeLimitInfo::TIMEOUT,
          std::string("协程任务超时: 出参为TIMEOUT, 实际=") + stateName(*state));
    check(c->timeout_cb_count == 1,
          "协程任务超时: 超时回调只执行一次, count=" + std::to_string(c->timeout_cb_count.load()));

    // 超时后任务不被强制终止, 仍会执行完, 但状态保持TIMEOUT
    bool done = waitValue(c->task_done, 0, 3000);
    check(done, "协程任务超时: 任务未被强制终止, 仍然执行完毕(协作式)");
    check(*state == TimeLimitInfo::TIMEOUT,
          std::string("协程任务超时: 任务结束后状态仍为TIMEOUT, 实际=") + stateName(*state));
}

// 3. 普通函数任务按时结束
static void testFuncFinishInTime(IOManager* iom, TimeManager* tim)
{
    auto c = std::make_shared<TaskCase>();
    auto state = std::make_shared<TimeLimitInfo::State>(TimeLimitInfo::WAITING);

    tim->addTaskWithTimeout([c]() { c->task_count++; c->task_done = 1; },
        1000, state, [c]() { c->timeout_cb_count++; });

    bool done = waitValue(c->task_done, 0, 3000);
    check(done, "函数任务按时结束: 任务已执行完");
    usleep(1200 * 1000);
    check(*state == TimeLimitInfo::FINISHED,
          std::string("函数任务按时结束: 出参为FINISHED, 实际=") + stateName(*state));
    check(c->timeout_cb_count == 0,
          "函数任务按时结束: 超时回调未执行, count=" + std::to_string(c->timeout_cb_count.load()));
}

// 4. 普通函数任务超时(由超时回调放行)
static void testFuncTimeout(IOManager* iom, TimeManager* tim)
{
    auto c = std::make_shared<TaskCase>();
    auto state = std::make_shared<TimeLimitInfo::State>(TimeLimitInfo::WAITING);
    g_release = 0;

    tim->addTaskWithTimeout(
        [c]() {
            c->task_count++;
            uint64_t begin = TimeManager::GetCurrentMS();
            while(!g_release.load() && TimeManager::GetCurrentMS() - begin < 5000)
            {
                usleep(1000);
            }
            c->task_done = 1;
        },
        200, state, [c]() { c->timeout_cb_count++; g_release = 1; });

    bool cb = waitValue(c->timeout_cb_count, 0, 2000);
    check(cb, "函数任务超时: 超时回调被执行");
    check(*state == TimeLimitInfo::TIMEOUT,
          std::string("函数任务超时: 出参为TIMEOUT, 实际=") + stateName(*state));

    bool done = waitValue(c->task_done, 0, 3000);
    check(done, "函数任务超时: 任务被放行后执行完毕");
    check(*state == TimeLimitInfo::TIMEOUT,
          std::string("函数任务超时: 任务结束后状态仍为TIMEOUT, 实际=") + stateName(*state));
}

// 5. 出参与超时回调均可为nullptr
static void testNullParams(IOManager* iom, TimeManager* tim)
{
    auto c = std::make_shared<TaskCase>();

    tim->addTaskWithTimeout(TaskCoro20::create_coro([c]() { return coroSleepTask(c, 500); }), 100, nullptr, nullptr);
    tim->addTaskWithTimeout([c]() { usleep(300 * 1000); c->task_done = 1; }, 100, nullptr, nullptr);

    bool done = waitValue(c->task_done, 0, 3000);
    check(done, "空参数: 不崩溃且任务正常执行");
    check(c->task_count == 1, "空参数: 协程任务已执行, count=" + std::to_string(c->task_count.load()));
}

int main(void)
{
    IOManager iom("task_timeout_test", 4);
    // TimeManager内部使用shared_from_this, 必须由shared_ptr持有
    TimeManager::ptr tim = std::make_shared<TimeManager>(&iom, 2000);
    tim->init();

    testCoroFinishInTime(&iom, tim.get());
    testCoroTimeout(&iom, tim.get());
    testFuncFinishInTime(&iom, tim.get());
    testFuncTimeout(&iom, tim.get());
    testNullParams(&iom, tim.get());

    iom.stop();
    std::cout << (g_failed ? "===== 存在问题, 失败用例数: " : "===== 全部用例通过, 失败用例数: ")
              << g_failed << std::endl;
    return g_failed ? 1 : 0;
}
