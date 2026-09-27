#include <catch2/catch_test_macros.hpp>

#include "basic/lock.hpp"
#include "basic/log.h"
#include "basic/timer/timer.hpp"
#include "coroutine/corobase.h"

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <memory>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

/**
    协程锁单元测试(以 CoUniqueLock 为主)

    每个用例在独立子进程中运行, 父进程(Catch2)做限时看门狗:
    用例若挂死(如死锁)会被判定为超时并杀掉子进程, 不会把 unit_tests 整体挂住,
    因此"逻辑失败"与"死锁"可以区分开。

    覆盖:
        1. CoMutex 基础: tryAcquire/unlock 语义(guard依赖的底层API)
        2. CoUniqueLock 基础: 加解锁循环, 析构与手动unlock两条释放路径
        3. CoUniqueLock 互斥: 多协程并发进入临界区, 计数不丢失且并发数恒为1
        4. CoUniqueLock 竞争: 持锁协程休眠期间其他协程排队, 之后依次获得锁(不死锁)
        5. CoUniqueLock 顺序: 等待者按FIFO顺序获得锁
        6. CoUniqueLock 手动解锁: 析构不得重复释放(不得放掉别人的锁)
        7. CoUniqueLock 压力: 多线程+多协程高并发下互斥与计数正确性
        8. LockAwaiter: 空闲锁上 co_await LockAwaiter, 即 await_ready() 直接命中的路径

    注意: 协程体必须写成具名协程函数并把捕获按值传参, 不能把带捕获的lambda直接当协程体
          (协程帧通过this引用闭包, create_coro返回后闭包悬垂)。
*/

using namespace m_sylar;

namespace {

Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");
int g_failed = 0;                                   // 当前子进程内失败的断言数

void check(bool ok, const std::string& what)
{
    std::cout << (ok ? "[ PASS ] " : "[ FAIL ] ") << what << std::endl;    // endl立即flush, 子进程被kill也不丢输出
    if(!ok) { ++g_failed; }
}

uint64_t nowMS()
{
    return TimeManager::GetCurrentMS();
}

// 等待原子变量达到期望值
bool waitFlag(std::atomic<int>& v, int expect, uint64_t timeout_ms)
{
    uint64_t begin = nowMS();
    while(v.load() != expect)
    {
        if(nowMS() - begin > timeout_ms) { return false; }
        usleep(1000);
    }
    return true;
}

// ------------------------- 被测对象与任务 -------------------------
struct Case
{
    CoMutex mutex;
    std::atomic<int> done {0};              // 执行完毕的协程数
    std::atomic<int> in_cs {0};             // 当前处于临界区的协程数
    std::atomic<int> max_in_cs {0};         // 临界区并发数峰值
    std::atomic<int> order_idx {0};         // 已获得锁的等待者序号
    std::atomic<int> order[8] {};           // 等待者获得锁的先后次序
    int protected_count = 0;                // 由锁保护的普通(int)计数器, 用于检测丢失更新
    std::atomic<int> flag1 {0};
    std::atomic<int> flag2 {0};
    std::atomic<int> flag3 {0};
};

// 进入临界区并记录并发情况
void enterCS(std::shared_ptr<Case>& c)
{
    int cur = ++c->in_cs;
    int prev = c->max_in_cs.load();
    while(cur > prev && !c->max_in_cs.compare_exchange_weak(prev, cur)) {}
}

// ---------- CoUniqueLock(guard) 任务 ----------

// guard保护临界区: 每次循环都由guard析构解锁
Task<void, TaskBeginExecuter> guardCriticalTask(std::shared_ptr<Case> c, int times, uint64_t hold_ms)
{
    for(int i = 0; i < times; i++)
    {
        {
            CoUniqueLock ul(c->mutex);
            co_await ul.lock();
            enterCS(c);
            if(hold_ms) { co_await co_sleep(hold_ms); }     // 临界区内挂起, 放大竞态
            c->protected_count++;                           // 非原子自增, 丢失更新即说明互斥失效
            --c->in_cs;
        }                                                   // 析构解锁
    }
    c->done++;
    co_return;
}

// guard加解锁循环: 偶数轮由析构释放, 奇数轮手动unlock(析构不应重复释放)
Task<void, TaskBeginExecuter> guardBasicTask(std::shared_ptr<Case> c, int times)
{
    for(int i = 0; i < times; i++)
    {
        CoUniqueLock ul(c->mutex);
        co_await ul.lock();
        if(i == 0)
        {
            c->flag1 = c->mutex.tryAcquire() ? 1 : 0;       // 期望0: 锁已被本guard持有
        }
        if(i % 2) { ul.unlock(); }                          // 手动释放
    }
    c->flag2 = c->mutex.tryAcquire() ? 1 : 0;               // 期望1: 全部释放
    if(c->flag2) { c->mutex.unlock(); }
    c->done++;
    co_return;
}

// guard取锁并记录次序
Task<void, TaskBeginExecuter> guardOrderedTask(std::shared_ptr<Case> c, int index,
                                              uint64_t delay_ms, uint64_t hold_ms)
{
    if(delay_ms) { co_await co_sleep(delay_ms); }
    {
        CoUniqueLock ul(c->mutex);
        co_await ul.lock();
        int idx = c->order_idx++;
        if(idx < 8) { c->order[idx] = index; }
        enterCS(c);
        if(hold_ms) { co_await co_sleep(hold_ms); }
        --c->in_cs;
    }
    c->done++;
    co_return;
}

// guard手动unlock后继续挂起, 期间别的协程持锁; 析构时不得重复释放
Task<void, TaskBeginExecuter> guardManualUnlockTask(std::shared_ptr<Case> c, uint64_t hold_ms)
{
    {
        CoUniqueLock ul(c->mutex);
        co_await ul.lock();
        co_await co_sleep(50);
        ul.unlock();                                        // 手动释放
        co_await co_sleep(hold_ms);                         // 期间holdProbeTask会拿到锁
    }                                                       // 析构: m_locked已为false, 不应再释放
    c->done++;
    co_return;
}

// ---------- CoMutex 底层API 任务 ----------

// 验证 tryAcquire 语义(全部在协程内, 避免主线程直接持锁)
Task<void, TaskBeginExecuter> tryAcquireTask(std::shared_ptr<Case> c)
{
    c->flag1 = c->mutex.tryAcquire() ? 1 : 0;               // 空闲 -> 成功
    c->flag2 = c->mutex.tryAcquire() ? 1 : 0;               // 已持有 -> 失败
    c->mutex.unlock();                                      // 释放
    c->flag3 = c->mutex.tryAcquire() ? 1 : 0;               // 已释放 -> 成功
    c->mutex.unlock();
    c->done++;
    co_return;
}

// 空闲锁上直接 co_await LockAwaiter, 命中 await_ready()==true 的路径
Task<void, TaskBeginExecuter> readyPathTask(std::shared_ptr<Case> c)
{
    co_await LockAwaiter(&c->mutex);                        // 锁空闲 -> 不挂起, 直接 await_resume
    c->flag1 = c->mutex.tryAcquire() ? 1 : 0;               // 期望0: 锁应已被本协程持有
    c->done++;
    c->mutex.unlock();
    c->flag2 = c->mutex.tryAcquire() ? 1 : 0;               // 期望1: 已释放
    c->mutex.unlock();
    co_return;
}

// 持锁一段时间(直接使用CoMutex, 用于探测他人持锁期间的可见状态)
Task<void, TaskBeginExecuter> holdProbeTask(std::shared_ptr<Case> c, uint64_t delay_ms, uint64_t hold_ms)
{
    if(delay_ms) { co_await co_sleep(delay_ms); }
    co_await c->mutex.lock();
    enterCS(c);
    c->flag1 = 1;                                           // 标记: 本协程正持锁
    co_await co_sleep(hold_ms);
    c->flag1 = 0;
    --c->in_cs;
    c->mutex.unlock();
    c->done++;
    co_return;
}

// 探测此刻能否取到锁(期望取不到)
Task<void, TaskBeginExecuter> tryProbeTask(std::shared_ptr<Case> c, uint64_t delay_ms)
{
    if(delay_ms) { co_await co_sleep(delay_ms); }
    c->flag2 = c->mutex.tryAcquire() ? 1 : 0;
    if(c->flag2) { c->mutex.unlock(); }
    c->done++;
    co_return;
}

// 一次性加解锁, 用于确认锁最终已释放
Task<void, TaskBeginExecuter> lockUnlockTask(std::shared_ptr<Case> c)
{
    {
        CoUniqueLock ul(c->mutex);
        co_await ul.lock();
    }
    c->done++;
    co_return;
}

// ------------------------- 用例 -------------------------

// 1. CoMutex 基础: tryAcquire/unlock 语义
int caseBasic()
{
    g_failed = 0;
    IOManager iom("co_mutex_basic", 4);
    auto c = std::make_shared<Case>();

    iom.schedule(TaskCoro20::create_coro([c]() { return tryAcquireTask(c); }));
    bool ok = waitFlag(c->done, 1, 3000);
    check(ok, "基础: tryAcquire 用例执行完毕");
    if(ok)
    {
        check(c->flag1 == 1, "基础: 空闲锁 tryAcquire 返回 true");
        check(c->flag2 == 0, "基础: 已持有锁 tryAcquire 返回 false");
        check(c->flag3 == 1, "基础: unlock 后 tryAcquire 返回 true");
    }
    iom.stop();
    return g_failed ? 1 : 0;
}

// 2. CoUniqueLock 基础: 加解锁循环, 覆盖析构与手动释放两条路径
int caseGuardBasic()
{
    g_failed = 0;
    IOManager iom("co_guard_basic", 4);
    auto c = std::make_shared<Case>();
    const int times = 100;

    iom.schedule(TaskCoro20::create_coro([c, times]() { return guardBasicTask(c, times); }));
    bool done = waitFlag(c->done, 1, 5000);
    check(done, "guard基础: " + std::to_string(times) + "次 guard 加解锁执行完毕, done="
                + std::to_string(c->done.load()));
    if(done)
    {
        check(c->flag1 == 0, "guard基础: 持锁期间 tryAcquire 返回 false(tryAcquire="
                             + std::to_string(c->flag1.load()) + ")");
        check(c->flag2 == 1, "guard基础: 全部释放后 tryAcquire 返回 true(tryAcquire="
                             + std::to_string(c->flag2.load()) + ")");
    }
    check(c->protected_count == 0, "guard基础: 未进入临界区, protected_count=0");
    iom.stop();
    return g_failed ? 1 : 0;
}

// 3. CoUniqueLock 互斥: 多协程并发进入临界区
int caseGuardMutualExclusion()
{
    g_failed = 0;
    IOManager iom("co_guard_excl", 4);
    auto c = std::make_shared<Case>();
    const int coro_num = 4;
    const int times = 250;

    for(int i = 0; i < coro_num; i++)
    {
        iom.schedule(TaskCoro20::create_coro([c, times]() { return guardCriticalTask(c, times, 0); }));
    }
    bool done = waitFlag(c->done, coro_num, 15000);
    check(done, "guard互斥: " + std::to_string(coro_num) + "个协程全部完成, done="
                + std::to_string(c->done.load()));
    if(done)
    {
        check(c->protected_count == coro_num * times,
              "guard互斥: 受保护计数无丢失, 期望=" + std::to_string(coro_num * times)
              + " 实际=" + std::to_string(c->protected_count));
    }
    check(c->max_in_cs == 1,
          "guard互斥: 临界区并发数峰值=1, 实际=" + std::to_string(c->max_in_cs.load()));

    // 全部结束后锁应已释放(析构已解锁)
    c->done = 0;
    iom.schedule(TaskCoro20::create_coro([c]() { return lockUnlockTask(c); }));
    check(waitFlag(c->done, 1, 2000), "guard互斥: 结束后锁已释放, 可再次获取");
    iom.stop();
    return g_failed ? 1 : 0;
}

// 4. CoUniqueLock 竞争: 持锁期间其他协程排队, 不死锁
int caseGuardContendedNoDeadlock()
{
    g_failed = 0;
    IOManager iom("co_guard_contend", 4);
    auto c = std::make_shared<Case>();
    uint64_t begin = nowMS();

    iom.schedule(TaskCoro20::create_coro([c]() { return guardCriticalTask(c, 1, 300); }));   // 持锁300ms
    iom.schedule(TaskCoro20::create_coro([c]() { return guardOrderedTask(c, 1, 50, 50); })); // 到期后抢锁
    iom.schedule(TaskCoro20::create_coro([c]() { return guardOrderedTask(c, 2, 50, 50); }));

    bool done = waitFlag(c->done, 3, 5000);
    uint64_t cost = nowMS() - begin;
    check(done, "guard竞争: 3个协程全部完成(未死锁), done=" + std::to_string(c->done.load())
                + " 耗时=" + std::to_string(cost) + "ms");
    check(c->max_in_cs <= 1, "guard竞争: 临界区未被并发进入, 峰值=" + std::to_string(c->max_in_cs.load()));
    check(cost >= 300 && cost <= 3000,
          "guard竞争: 耗时符合串行化预期(约400ms), 实际=" + std::to_string(cost) + "ms");
    iom.stop();
    return g_failed ? 1 : 0;
}

// 5. CoUniqueLock 顺序: 等待者按FIFO获得锁
int caseGuardFifoWakeup()
{
    g_failed = 0;
    IOManager iom("co_guard_fifo", 4);
    auto c = std::make_shared<Case>();

    // A先持锁400ms; B/C/D在50/150/250ms时依次进入等待队列
    iom.schedule(TaskCoro20::create_coro([c]() { return guardOrderedTask(c, 0, 0, 400); }));
    iom.schedule(TaskCoro20::create_coro([c]() { return guardOrderedTask(c, 1, 50, 0); }));
    iom.schedule(TaskCoro20::create_coro([c]() { return guardOrderedTask(c, 2, 150, 0); }));
    iom.schedule(TaskCoro20::create_coro([c]() { return guardOrderedTask(c, 3, 250, 0); }));

    bool done = waitFlag(c->done, 4, 6000);
    check(done, "guard顺序: 4个协程全部完成, done=" + std::to_string(c->done.load()));
    if(done)
    {
        check(c->order[0] == 0 && c->order[1] == 1 && c->order[2] == 2 && c->order[3] == 3,
              "guard顺序: 获得锁次序为 0,1,2,3(FIFO), 实际=" + std::to_string(c->order[0].load()) + ","
              + std::to_string(c->order[1].load()) + "," + std::to_string(c->order[2].load()) + ","
              + std::to_string(c->order[3].load()));
    }
    check(c->max_in_cs == 1,
          "guard顺序: 临界区并发数峰值=1, 实际=" + std::to_string(c->max_in_cs.load()));
    iom.stop();
    return g_failed ? 1 : 0;
}

// 6. CoUniqueLock 手动解锁: 析构不得重复释放(不得放掉别人的锁)
int caseGuardManualUnlock()
{
    g_failed = 0;
    IOManager iom("co_guard_manual", 4);
    auto c = std::make_shared<Case>();

    iom.schedule(TaskCoro20::create_coro([c]() { return guardManualUnlockTask(c, 300); }));
    iom.schedule(TaskCoro20::create_coro([c]() { return holdProbeTask(c, 100, 200); }));
    iom.schedule(TaskCoro20::create_coro([c]() { return tryProbeTask(c, 200); }));

    bool done = waitFlag(c->done, 3, 4000);
    check(done, "guard手动解锁: 3个协程全部完成, done=" + std::to_string(c->done.load()));
    check(c->flag2 == 0,
          "guard手动解锁: 他人持锁期间探测取锁失败(tryAcquire=0), 实际=" + std::to_string(c->flag2.load()));
    check(c->max_in_cs == 1,
          "guard手动解锁: 临界区并发数峰值=1, 实际=" + std::to_string(c->max_in_cs.load()));
    iom.stop();
    return g_failed ? 1 : 0;
}

// 7. CoUniqueLock 压力: 多线程+多协程高并发
int caseGuardStress()
{
    g_failed = 0;
    IOManager iom("co_guard_stress", 8);
    auto c = std::make_shared<Case>();
    const int coro_num = 8;
    const int times = 1000;

    for(int i = 0; i < coro_num; i++)
    {
        iom.schedule(TaskCoro20::create_coro([c, times]() { return guardCriticalTask(c, times, 0); }));
    }
    bool done = waitFlag(c->done, coro_num, 20000);
    check(done, "guard压力: " + std::to_string(coro_num) + "协程 x " + std::to_string(times)
                + "次全部完成, done=" + std::to_string(c->done.load()));
    if(done)
    {
        check(c->protected_count == coro_num * times,
              "guard压力: 受保护计数无丢失, 期望=" + std::to_string(coro_num * times)
              + " 实际=" + std::to_string(c->protected_count));
    }
    check(c->max_in_cs == 1,
          "guard压力: 临界区并发数峰值=1, 实际=" + std::to_string(c->max_in_cs.load()));
    iom.stop();
    return g_failed ? 1 : 0;
}

// 8. LockAwaiter: 空闲锁上直接 co_await, 命中 await_ready()==true 的路径
int caseAwaiterReadyPath()
{
    g_failed = 0;
    IOManager iom("co_guard_ready", 4);
    auto c = std::make_shared<Case>();

    iom.schedule(TaskCoro20::create_coro([c]() { return readyPathTask(c); }));

    bool ok = waitFlag(c->done, 1, 3000);
    check(ok, "awaiter: await_ready 命中路径未崩溃且执行完毕");
    if(ok)
    {
        check(c->flag1 == 0, "awaiter: await_ready 成功后锁处于已持有状态");
        check(c->flag2 == 1, "awaiter: unlock 后锁被正确释放");
    }
    iom.stop();
    return g_failed ? 1 : 0;
}

// ------------------------- 子进程隔离 + 看门狗 -------------------------
enum CaseStatus
{
    CASE_PASS = 0,          // 子进程内所有检查点通过
    CASE_FAIL = 1,          // 有检查点失败
    CASE_TIMEOUT = 2,       // 超时未结束(疑似死锁), 已被杀掉
    CASE_CRASH = 3,         // 子进程被信号终止
};

struct CaseRun
{
    int status {CASE_FAIL};
    int exit_code {0};
    int signo {0};
    uint64_t cost_ms {0};
};

CaseRun runIsolated(int (*fn)(), uint64_t timeout_ms)
{
    CaseRun r;
    // 调试用: CO_MUTEX_DIRECT=1 时不fork, 便于gdb直接跟进用例本体
    if(getenv("CO_MUTEX_DIRECT") != nullptr)
    {
        r.exit_code = fn();
        r.status = (r.exit_code == 0) ? CASE_PASS : CASE_FAIL;
        return r;
    }

    fflush(stdout);
    pid_t pid = fork();
    if(pid == 0)
    {
        int rt = fn();
        fflush(stdout);
        _exit(rt);                  // 不走atexit/Catch2清理
    }
    if(pid < 0)
    {
        r.status = CASE_FAIL;
        return r;
    }

    uint64_t begin = nowMS();
    int status = 0;
    while(true)
    {
        pid_t w = waitpid(pid, &status, WNOHANG);
        if(w == pid) { break; }
        if(nowMS() - begin > timeout_ms)
        {
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            r.status = CASE_TIMEOUT;
            r.cost_ms = nowMS() - begin;
            return r;
        }
        usleep(5000);
    }
    r.cost_ms = nowMS() - begin;
    if(WIFEXITED(status))
    {
        r.exit_code = WEXITSTATUS(status);
        r.status = (r.exit_code == 0) ? CASE_PASS : CASE_FAIL;
    }
    else if(WIFSIGNALED(status))
    {
        r.signo = WTERMSIG(status);
        r.status = CASE_CRASH;
    }
    return r;
}

std::string describe(const CaseRun& r)
{
    switch(r.status)
    {
        case CASE_PASS:    return "子进程通过, 耗时 " + std::to_string(r.cost_ms) + "ms";
        case CASE_TIMEOUT: return "用例挂死(疑似死锁), 超过限时未结束, 已杀掉子进程, 耗时 "
                                  + std::to_string(r.cost_ms) + "ms";
        case CASE_CRASH:   return "子进程被信号 " + std::to_string(r.signo)
                                  + " 终止(崩溃/abort), 耗时 " + std::to_string(r.cost_ms) + "ms";
        default:           return "子进程内有检查点失败, 退出码=" + std::to_string(r.exit_code)
                                  + ", 耗时 " + std::to_string(r.cost_ms) + "ms";
    }
}

}   // namespace

TEST_CASE("CoMutex 基础: tryAcquire 与 unlock 语义", "[co_mutex]")
{
    CaseRun r = runIsolated(caseBasic, 10000);
    INFO(describe(r));
    REQUIRE(r.status == CASE_PASS);
}

TEST_CASE("CoUniqueLock 基础: 加解锁循环与两条释放路径", "[co_mutex]")
{
    CaseRun r = runIsolated(caseGuardBasic, 10000);
    INFO(describe(r));
    REQUIRE(r.status == CASE_PASS);
}

TEST_CASE("CoUniqueLock 互斥: 多协程并发临界区", "[co_mutex]")
{
    CaseRun r = runIsolated(caseGuardMutualExclusion, 15000);
    INFO(describe(r));
    REQUIRE(r.status == CASE_PASS);
}

TEST_CASE("CoUniqueLock 竞争: 持锁期间排队不死锁", "[co_mutex]")
{
    CaseRun r = runIsolated(caseGuardContendedNoDeadlock, 8000);
    INFO(describe(r));
    REQUIRE(r.status == CASE_PASS);
}

TEST_CASE("CoUniqueLock 顺序: 等待者FIFO获得锁", "[co_mutex]")
{
    CaseRun r = runIsolated(caseGuardFifoWakeup, 8000);
    INFO(describe(r));
    REQUIRE(r.status == CASE_PASS);
}

TEST_CASE("CoUniqueLock 手动解锁: 析构不重复释放", "[co_mutex]")
{
    CaseRun r = runIsolated(caseGuardManualUnlock, 8000);
    INFO(describe(r));
    REQUIRE(r.status == CASE_PASS);
}

TEST_CASE("CoUniqueLock 压力: 8协程x1000次高并发", "[co_mutex]")
{
    CaseRun r = runIsolated(caseGuardStress, 30000);
    INFO(describe(r));
    REQUIRE(r.status == CASE_PASS);
}

TEST_CASE("LockAwaiter: await_ready 直接命中", "[co_mutex]")
{
    CaseRun r = runIsolated(caseAwaiterReadyPath, 8000);
    INFO(describe(r));
    REQUIRE(r.status == CASE_PASS);
}
