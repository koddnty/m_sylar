# 定时器模块

## 模块介绍

​	定时器模块基于 Linux 的 timerfd 机制实现，通过 IOManager 进行事件注册与触发，提供毫秒级的异步定时能力。模块采用**时间轮（Time Wheel）**算法组织定时任务——将任务按执行时间分片存入 TimerBlock，到期时批量取出调度执行，减少锁竞争和系统调用开销。

​	模块支持两种定时器类型：

- **普通定时器**：到期后直接执行回调，可选循环或单次。
- **条件定时器**：到期后先执行条件判断，条件成立执行主回调，否则执行备选回调。

​	此外，通过 `addEventWithTimeout` 可为任意 IO 事件添加超时限制，方便超时系统开发。

​	需要注意的是，本框架并未采用单例模式，由于定时器模块依赖于 IOManager 调度，在使用前需初始化并绑定到指定的 IOManager 实例。定时器模块关闭时建议先停止 IOManager 再析构 TimeManager，确保 timerfd 事件被正确清理。

## 快速开始

```cpp
#include <iostream>
#include "basic/timer/timer.hpp"
#include "coroutine/corobase.h"

using namespace m_sylar;

// 普通定时器 — 1ms 后执行一次
Task<void, TaskBeginExecuter> testBaseTimer() {
    IOManager iom {"test_timer", 1};
    TimeManager::ptr tim = std::make_shared<TimeManager>(&iom, 2000);
    tim->init();

    TimeTask::ptr task = TimeTask::create(1, false,
        [](TimeTask::ptr) -> Task<void> {
            std::cout << "timer fired!" << std::endl;
            co_return;
        });
    tim->addTimer(task);

    sleep(1);
    iom.stop();
    co_return;
}

// 条件定时器 — 每 1ms 检查条件，满足后执行
Task<void, TaskBeginExecuter> testConditionTimer() {
    IOManager iom {"test_condition", 1};
    TimeManager::ptr tim = std::make_shared<TimeManager>(&iom, 2000);
    tim->init();

    std::atomic<int> counter {0};
    TimeTask::ptr task = TimeTask::create(1, true,
        // 主回调：条件满足后执行
        [&counter](TimeTask::ptr task) -> Task<void> {
            counter++;
            if (counter >= 1000) task->cancel();
            co_return;
        },
        // 条件：返回 true 才执行主回调
        [&counter]() -> Task<bool> {
            co_return counter < 1000;
        },
        // 备选回调：条件不满足时执行
        [](TimeTask::ptr) -> Task<void> {
            co_return;
        });
    tim->addConditionTimer(task);

    sleep(3);
    iom.autoStop();
    co_return;
}
```

## API参考

| 函数/接口名称                                                | 功能                           | 所属类        | 等级   |
| :----------------------------------------------------------- | ------------------------------ | ------------- | ------ |
| [TimeTask::create(...)](#timetaskcreate)                     | 创建一个定时器任务             | TimeTask      | 用户   |
| [TimeManager(IOManager::ptr, size_t)](#timemanager构造函数)  | 构造一个定时器管理器           | TimeManager   | 用户   |
| [TimeManager::init()](#timemanagerinit)                      | 初始化 timerfd 并注册到 IOManager | TimeManager | 用户   |
| [TimeManager::close()](#timemanagerclose)                    | 关闭定时器，取消事件监听       | TimeManager   | 用户   |
| [addTimer(TimeTask::ptr)](#addtimertimetaskptr)              | 添加普通定时器任务             | TimeManager   | 用户   |
| [addConditionTimer(TimeTask::ptr)](#addconditiontimertimetaskptr) | 添加条件定时器任务          | TimeManager   | 用户   |
| [cancelTimer(TimeTask::ptr)](#canceltimertimetaskptr)        | 取消一个定时器任务             | TimeManager   | 用户   |
| [getTimerCount()](#gettimercount)                            | 获取当前活跃的定时器任务数量   | TimeManager   | 用户   |
| [addEventWithTimeout(...)](#addeventwithtimeout)             | 为 IO 事件添加超时限制         | TimeManager   | 用户   |
| [getInstance()](#getinstance)                                | 获取线程局部的 TimeManager 实例 | TimeManager  | 用户   |
| [GetCurrentMS()](#getcurrentms)                              | 获取当前时间戳（毫秒）         | TimeManager   | 用户   |
| [printInfo()](#printinfo)                                    | 打印调试信息（状态诊断）       | TimeManager   | 开发者 |
| [onTimerTriggered()](#ontimertriggered)                      | timerfd 到期回调               | TimeManager   | 开发者 |

---

### TimeTask

#### TimeTask::create()

**函数：**

```cpp
static TimeTask::ptr create(uint64_t ms, bool is_cycle,
    std::function<Task<void>(TimeTask::ptr task)> main_cb,
    std::function<Task<bool>()> condition = nullptr,
    std::function<Task<void>(TimeTask::ptr task)> condition_cb = nullptr);
```

**参数：**
- `ms`：定时间隔，单位**毫秒**。任务将在 `当前时间 + ms` 的时刻首次执行。
- `is_cycle`：是否循环执行。`true` 则任务执行后自动重新插入时间片，`false` 则仅执行一次。
- `main_cb`：主回调函数，定时器到期后执行。函数签名需为 `Task<void>(TimeTask::ptr)` 的协程函数。
- `condition`：条件判断函数（可选，仅条件定时器使用）。到期后先调用此函数，返回 `true` 才执行 `main_cb`，否则执行 `condition_cb`。函数签名为 `Task<bool>()`。
- `condition_cb`：条件不满足时的备选回调函数（可选）。

**描述：** 工厂方法，创建一个 TimeTask 对象并自动计算首次执行时间。普通定时器只需传前三个参数，条件定时器需额外传 `condition` 和 `condition_cb`。

**备注:** `` std::function<Task<void>(TimeTask::ptr task)> main_cb``中,函数参数传入的是注册时的定时器智能指针,可通过此智能指针停止定时器.

**返回值：** 新创建的 `TimeTask::ptr`。

---

### TimeManager

#### TimeManager 构造函数

**函数：**

```cpp
TimeManager(IOManager::ptr iom, size_t blockLength);
TimeManager(IOManager* iom, size_t blockLength);
```

**参数：**
- `iom`：绑定的 IOManager 实例（智能指针或裸指针）。定时器通过该 IOManager 注册 timerfd 事件和调度回调协程。
- `blockLength`：时间片（TimerBlock）长度，单位**毫秒**。任务按此粒度分桶，值越小块内锁竞争越少，但时间片数量增多,manager锁变多；值越大则反之。默认推荐 2000-3000ms。可通过配置项 `basic.timer.blocklength` 全局调整。

**描述：** 构造 TimeManager 但不启动。需要随后调用 `init()` 完成 timerfd 创建和事件注册。

---

#### TimeManager::init()

**函数：**

```cpp
int init();
```

**描述：** 创建 timerfd（`CLOCK_MONOTONIC`，非阻塞），将其 READ 事件注册到绑定的 IOManager 上，回调为 `onTimerTriggered()`。同时记录基准时间 `m_base_time` 用于后续时间片计算。

**返回值：** `0` 成功；`-1` 失败（timerfd 创建失败时打印错误日志）。

---

#### TimeManager::close()

**函数：**

```cpp
int close();
```

**描述：** 从 IOManager 中删除 timerfd 的 READ 事件监听。调用后定时器不再触发，但已调度执行的协程仍会继续运行。

**返回值：** `0` 成功。

---

#### addTimer(TimeTask::ptr)

**函数：**

```cpp
TimeTask::ptr addTimer(TimeTask::ptr time_task);
```

**参数：** `time_task`：由 `TimeTask::create()` 创建的任务对象。

**描述：** 添加一个普通定时器。调用 `insertTimeTask()` 将任务插入对应的时间片，必要时更新 timerfd 的到期时间。如果任务的执行时间已过期，则直接调度执行而不插入时间片。

**返回值：** 返回传入的 `time_task`，可用于后续取消操作。

---

#### addConditionTimer(TimeTask::ptr)

**函数：**

```cpp
TimeTask::ptr addConditionTimer(TimeTask::ptr time_task);
```

**参数：** `time_task`：由 `TimeTask::create()` 创建且包含 `condition` 和 `condition_cb` 的任务对象。

**描述：** 添加一个条件定时器。与 `addTimer` 类似，但会检查任务是否设置了条件函数——条件函数和备选回调均为必须，否则打印错误日志。

**返回值：** 返回传入的 `time_task`；条件不完整时返回 `nullptr`。

---

#### cancelTimer(TimeTask::ptr)

**函数：**

```cpp
void cancelTimer(TimeTask::ptr time_task);
```

**参数：** `time_task`：待取消的任务。

**描述：** 取消一个定时器任务。调用任务的 `cancel()` 方法将其标记为取消状态，后续 `runTimeTask` 协程执行时会检查此标记并跳过循环重插入。

---

#### getTimerCount()

**函数：**

```cpp
inline size_t getTimerCount() const;
```

**描述：** 获取当前已插入时间片且尚未调度的定时器任务数量（不含通过“已过期直接调度”路径执行的任务）。

**返回值：** 当前活跃的定时器任务计数。

---

#### addEventWithTimeout()

**函数：**

```cpp
// 协程任务版本
IOManager& addEventWithTimeout(int fd, FdContext::Event event, TaskCoro20&& task,
                               uint64_t timeout, std::shared_ptr<TimeLimitInfo::State> rtState,
                               int closeFlag = 0);

// 普通函数版本
IOManager& addEventWithTimeout(int fd, FdContext::Event event, std::function<void()> cb_func,
                               uint64_t timeout, std::shared_ptr<TimeLimitInfo::State> rtState,
                               int closeFlag = 0);
```

**参数：**
- `fd`：目标文件描述符。
- `event`：监听的 IO 事件类型（`FdContext::Event::READ` 或 `FdContext::Event::WRITE`）。
- `task` / `cb_func`：IO 事件到达或超时后执行的回调。
- `timeout`：超时时间，单位**毫秒**。
- `rtState`：出参，用于获取最终执行原因。值为 `TimeLimitInfo::State::FINISHED`（IO 事件先到达）或 `TimeLimitInfo::State::TIMEOUT`（超时先触发）。
- `closeFlag`：超时时是否同时 close fd。`0` 仅删除事件监听；非 `0` 则调用 `closeWithNoClose()`。

**描述：** 为指定的 IO 事件添加超时限制。内部创建条件定时器，同时监听 IO 事件和超时事件——谁先完成谁先通过 CAS 抢占执行权，确保回调仅执行一次。这是实现带超时的 `co_accept`、`co_read` 等 hook 函数的基础。

**返回值：** 当前 IOManager 的引用，用于链式调用。

---

#### getInstance()

**函数：**

```cpp
static std::shared_ptr<TimeManager> getInstance();
```

**描述：** 获取当前线程局部的 TimeManager 实例（`thread_local` 存储）。首次调用时自动创建并初始化，使用配置项 `basic.timer.blocklength` 作为默认时间片长度。主要用于协程 hook 模块（如 `co_sleep`）自动获取定时器。

​	线程局部的定时器任务,基于当前线程的iomanager实例,通常来说只是为内部定时任务使用,外部使用请主动创建.在iomanager线程池外线程使用此函数会引发段错误,因为主线程不属于iomanager,没有iomanager对象实例进行timemanager的构建.

**返回值：** 线程局部的 `TimeManager::ptr`。

---

#### GetCurrentMS()

**函数：**

```cpp
static uint64_t GetCurrentMS();
```

**描述：** 获取当前 `CLOCK_MONOTONIC` 时间戳。

**返回值：** 自系统启动以来的毫秒数。

---

### 内部接口

以下接口为内部实现，供开发者参考。

#### onTimerTriggered()

**函数：**

```cpp
void onTimerTriggered();
```

**描述：** timerfd 到期时的回调（由 IOManager 在 timerfd 可读时调用）。执行流程：

1. 扫描 `m_time_blocks`，收集所有起始时间 ≤ 当前时间的 TimerBlock。
2. 从各 TimerBlock 中取出已过期的任务（`getAndRemove()`）。
3. 清理已完全过期的时间片（`end_time <= now`）。
4. 将 `m_nextTimerTime` 重置为 `UINT64_MAX` 后调用 `updateTimerFd()` 重新 arm timerfd 到下一个最早任务的时间。
5. 将所有过期任务通过 `m_iom->schedule()` 调度为协程执行。

#### insertTimeTask(TimeTask::ptr)

```cpp
int insertTimeTask(TimeTask::ptr time_task);
```

**描述：** 将任务插入时间片或直接调度。
- 若 `execute_time <= now`（已过期），直接通过 `schedule()` 调度执行，不进入时间片，不增加 `m_timerCount`。
- 否则，调用 `getOrCreateTimerBlock()` 获取/创建对应的时间片并插入，同时检查是否需要更新 timerfd 的到期时间（新任务时间 < 当前 armed 时间）。

**返回值：** `0` 成功；`-1` 失败。

#### updateTimerFd(uint64_t execute_time)

```cpp
int updateTimerFd(uint64_t execute_time);
```

**描述：** 调用 `timerfd_settime()` 设置 timerfd 的到期时间。内置防回退检查：若 `execute_time >= m_nextTimerTime` 则跳过（timerfd 已 arm 在更早时间）。调用前需确保 `m_nextTimerTime` 在 timerfd 到期后已被重置（`onTimerTriggered()` 中置为 `UINT64_MAX`），否则可能永不 re-arm。

**返回值：** `0` 成功或无需更新；`-1` 系统调用失败。

#### printInfo()

**函数：**

```cpp
void printInfo();
```

**描述：** 打印 TimeManager 当前状态至 stdout，用于调试诊断。输出内容包括：timerfd、任务计数、时间片数量、next timer time、当前时间、时间片大小、基准时间，以及每个 TimerBlock 的起止时间和任务数。

## 架构概述

```
TimeManager
  ├── timerfd (CLOCK_MONOTONIC, TFD_NONBLOCK)
  │     └── 通过 IOManager::addEvent 注册 EPOLLIN 回调
  ├── m_time_blocks: multimap<uint64_t, TimerBlock::ptr>
  │     └── 时间轮：按 block 起始时间索引
  ├── TimerBlock
  │     ├── start_time / end_time (2000ms 默认窗口)
  │     └── time_tasks: multimap<uint64_t, TimeTask::ptr>
  │           └── 按执行时间排序的定时任务
  └── TimeTask
        ├── m_main_cb (主回调)
        ├── m_condition / m_condition_cb (条件判断/备选回调)
        ├── m_is_cycle (是否循环)
        └── m_is_canceled (是否已取消)
```

**执行流程：**

```
timerfd 到期
  → IOManager epoll 检测到 timerfd 可读
  → onTimerTriggered()
       → 收集过期的 TimerBlock
       → getAndRemove(now) 取出过期任务
       → 清理空的时间片
       → 重置 m_nextTimerTime = UINT64_MAX
       → updateTimerFd(next_time) 重新 arm timerfd
       → schedule() 调度所有过期任务为协程
            → runTimeTask() 协程执行
                 → co_await timetask->runner() 执行回调
                 → 若循环且未取消 → insertTimeTask() 重新插入
```

## 配置项

| 配置项                    | 默认值 | 描述                                   |
| ------------------------- | ------ | -------------------------------------- |
| `basic.timer.blocklength` | 3000   | 默认单个时间块的时间长度，单位 ms。越小锁竞争越少，但时间片数量增多。 |
