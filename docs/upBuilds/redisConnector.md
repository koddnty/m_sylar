# Redis 数据库连接池模块

## 模块介绍

​	Redis 数据库连接池模块基于 [hiredis](https://github.com/redis/hiredis)（Redis 官方 C 客户端）实现，通过 `DBPool` 模板基类统一管理连接的借出、归还与扩容，再结合 C++20 协程封装为 `co_await` 接口。

​	**需要注意：** 与 MySQL 连接池不同，本模块使用 hiredis 的**同步阻塞接口** `redisCommand()` 执行命令。虽然 `executeQuery()` 本身以协程形式提供 `co_await` 接口，但底层命令执行是阻塞的——当命令到达后由当前协程直接向 Redis 发送并等待响应，期间不会像 MySQL 那样挂起协程去监听 fd。因此：

- 在协程中**串行执行少量命令**没有问题，多条协程通过连接池分时复用连接。
- 单个命令的耗时（如 `KEYS *`、大 `MGET`）会**阻塞当前调度线程**，应避免在 IOManager 调度线程上执行重命令。
- Redis 连接是**无状态的**（协议层不维护会话），单条命令原子执行，复用连接无需像 MySQL 那样回滚事务，因此本模块执行完毕后直接归还连接。

​	与 MySQL 连接池相同，本框架并未采用单例模式。连接池依赖 IOManager 调度，使用前需初始化并绑定到具体的 IOManager 实例，让用到连接池的任务通过 IOManager 的 `schedule()` 方法调度即可。关闭时同样建议先停止 IOManager 再关闭连接池。

## 快速开始

```cpp
#include <iostream>
#include "DBPool/redis.h"
#include "DBPool/factory.h"
#include "coroutine/corobase.h"

using namespace m_sylar;

// 在协程中异步执行 Redis 命令
Task<void, TaskBeginExecuter> test(RedisPoolManager* redisPool) {
    // 简单字符串命令
    RedisResp::ptr setResp = co_await redisPool->executeQuery("SET name sylar");
    if(setResp) {
        std::cout << "SET: " << setResp->asString() << std::endl;   // >>> OK
    }

    // 读取并转换类型
    RedisResp::ptr getResp = co_await redisPool->executeQuery("GET name");
    if(getResp) {
        std::cout << "GET: " << getResp->asString() << std::endl;   // >>> sylar
    }

    // 数组返回（如 LRANGE / HGETALL / SMEMBERS）
    RedisResp::ptr lResp = co_await redisPool->executeQuery("LRANGE mylist 0 -1");
    if(lResp && lResp->getType() == REDIS_REPLY_ARRAY) {
        for(const auto& item : lResp->asArray()) {
            std::cout << "item: " << item->asString() << std::endl;
        }
    }
}

int main(void) {
    RedisPoolManager::ptr redis_pool = DB::createRedisPool(10, 15); // 创建公共连接池
    if(-1 == redis_pool->init("localhost", 6379)) {
        std::cout << "failed to init redis pool" << std::endl;
        return -1;
    }
    std::cout << "init redis pool succeed" << std::endl;

    IOManager iom("test_redis_pool", 1);        // 所有查询任务需通过 iom 调度

    for(int i = 0; i < 30; i++) {
        iom.schedule(TaskCoro20::create_coro(std::bind(test, redis_pool.get())));
    }

    sleep(3);                   // 等待协程任务执行完毕
    iom.autoStop();             // 先停止 IOManager
    redis_pool->close();        // 再关闭连接池
    return 0;
}
```

## API参考

| 函数/接口名称                                                | 功能                             | 所属类           | 等级   |
| :----------------------------------------------------------- | -------------------------------- | ---------------- | ------ |
| [RedisPoolManager(int min_conn, int max_conn)](#redispoolmanager-构造函数) | 创建一个连接池                   | RedisPoolManager | 用户   |
| [RedisPoolManager::init(...)](#redispoolmanagerinit)         | 初始化连接池，连接到 Redis 服务器 | RedisPoolManager | 用户   |
| [RedisPoolManager::close()](#redispoolmanagerclose)          | 关闭数据库连接池                 | RedisPoolManager | 用户   |
| [executeQuery(const std::string&)](#executequerystdstring)   | 异步执行一条 Redis 命令          | RedisPoolManager | 用户   |
| [borrowOneConn()](#borrowoneconn)                            | 借出一条空闲连接                 | RedisPoolManager | 开发者 |
| [checkRunState()](#checkrunstate)                            | 检查连接池运行状态               | RedisPoolManager | 用户   |
| [registeConnCb(...)](#registeconncb)                         | 注册连接资源可用任务回调         | RedisPoolManager | 开发者 |
| [tickle()](#tickle)                                          | 唤醒等待中的回调，与 registeConnCb 配合 | RedisPoolManager | 开发者 |
| [RedisResp::getType()](#redisrespgettype)                    | 获取回复类型                     | RedisResp        | 用户   |
| [RedisResp::asInt()](#redisrespasint)                        | 以整型读取回复                   | RedisResp        | 用户   |
| [RedisResp::asDouble()](#redisrespasdouble)                  | 以浮点读取回复                   | RedisResp        | 用户   |
| [RedisResp::asString()](#redisrespasstring)                  | 以字符串读取回复                 | RedisResp        | 用户   |
| [RedisResp::asArray()](#redisrespasarray)                    | 以数组读取回复                   | RedisResp        | 用户   |
| [RedisResp::getState()](#redisrespgetstate)                  | 获取 IO 执行状态                 | RedisResp        | 用户   |

---

### 工厂方法 DB

​	连接池可通过 `DBPool/factory.h` 提供的工厂方法创建，支持公共实例（单例）与私有实例两种管理方式。

#### DB::createRedisPool(min_conn, max_conn, is_public)

**函数：**

```cpp
static RedisPoolManager::ptr createRedisPool(int min_conn, int max_conn, bool is_public = true);
```

**参数：** `min_conn` 最小连接数量；`max_conn` 最大连接数量；`is_public` 为 `true` 时注册为公共实例，可通过 `DB::Redis::getInstance()` 全局获取，为 `false` 时返回私有实例需自行管理。

**描述：** 创建一个 Redis 连接池。**注意：工厂方法仅创建连接池对象，仍需手动调用 `init()` 完成连接。**

#### DB::Redis::getInstance()

**函数：**

```cpp
static RedisPoolManager::ptr getInstance();
```

**描述：** 获取公共 Redis 连接池实例。**⚠️ 注意：当前实现存在缺陷**——内部错误地调用了 `createMysqlPool(5, 10, true)` 而非 `createRedisPool`，导致 `RedisPoolInstance` 始终为 `nullptr`，返回空指针。建议使用 `DB::createRedisPool(...)` 创建并自行持有管理，而非依赖此接口。

---

### RedisPoolManager

#### RedisPoolManager 构造函数

**函数：**

```cpp
RedisPoolManager(int min_conn, int max_conn);
```

**参数：** `min_conn` 最小连接数量；`max_conn` 最大连接数量。

**描述：** 构造函数继承 `DBPool<RedisConn, RedisResp>`，一次性初始化所有（`max_conn`）数量的 `RedisConn` 对象，但仅连接 `min_conn` 数量的连接。请确保 `0 <= min_conn <= max_conn`，否则抛出 `std::runtime_error`。

#### RedisPoolManager::init(...)

**函数：**

```cpp
int init(const std::string& host, int port);
```

**参数：** `host` Redis 服务器地址；`port` Redis 服务端口（默认 6379）。

**描述：** 记录连接信息并建立 `min_conn` 个最小连接。任一连接失败则状态置为 `ERROR`，返回 `-1` 并打印错误日志；成功返回 `0`，状态置为 `READY`。

#### RedisPoolManager::close()

**函数：**

```cpp
void close();
```

**描述：** 将连接池状态置为 `CLOSING` 并唤醒所有等待回调，等待中的协程会以失败结果返回。由于底层为 hiredis 同步接口，连接由 hiredis 内部管理，建议先停止 IOManager 再调用 `close()`，避免调度线程仍在执行命令时连接被释放。

#### executeQuery(const std::string&)

**函数：**

```cpp
Task<std::shared_ptr<RedisResp>> executeQuery(const std::string& query);
```

**参数：** `query` 将要执行的 Redis 命令（与 `redisCommand()` 一致，支持格式化参数，如 `"SET key value"`）。

**描述：** 异步执行一条 Redis 命令。执行流程为：借出一条空闲连接 → 通过 `redisCommand()` 同步执行命令 → 归还连接。线程安全。

**返回值：** 命令正常返回 `RedisResp::ptr` 响应封装；若连接池处于未运行状态，内部会构造 `RedisResp(nullptr)` 触发断言失败（详见[错误](#错误)），因此**调用前请确保连接池已 `init()` 且未 `close()`**。

#### borrowOneConn()

**函数：**

```cpp
virtual ConnectWrapper<RedisConn, RedisResp>::ptr borrowOneConn();
```

**描述：** 借出一条空闲连接，返回连接包装器。包装器析构时自动将连接归还连接池。连接池忙或状态非法时返回 `conn_idx == -1` 的空包装器。**线程不安全**，请仅在协程内使用。

#### checkRunState()

**函数：**

```cpp
[[nodiscard]] bool checkRunState() const;
```

**描述：** 检查连接池运行状态。`READY` 与 `FULL` 返回 `true`；`INIT`、`CLOSED`、`CLOSING`、`ERROR` 返回 `false`。

#### registeConnCb(...)

**函数：**

```cpp
virtual int registeConnCb(const std::function<void()>& cb);
```

**描述：** 维护一个回调队列。当有可用资源时直接通过当前线程的 IOManager 调度执行注册的回调；若连接耗尽（`FULL`），将回调加入队列，待有连接归还时回调任务。内部用于 `GetConnAwaiter` 实现连接等待。

**参数：** `cb` 当连接池有新的连接可用时执行的回调操作。

**返回值：** `0` 成功；`-1` 运行失败并打印错误日志。

#### tickle()

**函数：**

```cpp
virtual int tickle();
```

**描述：** 线程安全函数。新资源到来时触发固定数量（`min(空闲连接数, 等待回调数)`）个注册的回调，并通过 IOManager 接口异步执行。连接池处于 `CLOSING` 时唤醒全部回调。

**返回值：** `0` 成功；`-1` 运行失败并打印错误日志。

---

### RedisResp

**描述：** Redis 命令回复的封装，内部持有 `redisReply*`，析构时自动调用 `freeReplyObject()` 释放（数组子元素除外，见 [asArray()](#redisrespasarray)）。类型常量与 hiredis 一致：

| 类型常量             | 值 | 说明                         |
| -------------------- | -- | ---------------------------- |
| `REDIS_REPLY_STRING` | 1  | 字符串                       |
| `REDIS_REPLY_ARRAY`  | 2  | 数组（如 `LRANGE`、`HGETALL`） |
| `REDIS_REPLY_INTEGER`| 3  | 整型（如 `INCR`、`EXPIRE` 的返回） |
| `REDIS_REPLY_NIL`    | 4  | 空值（如不存在的 key）        |
| `REDIS_REPLY_STATUS` | 5  | 状态回复（如 `SET` 返回 `OK`）|
| `REDIS_REPLY_ERROR`  | 6  | 错误回复（如命令语法错误）    |
| `REDIS_REPLY_DOUBLE` | 7  | 浮点（如 `ZSCORE`）           |
| `REDIS_REPLY_PUSH`   | 12 | 推送消息（RESP3）             |
| `REDIS_REPLY_BIGNUM` | 13 | 大整数                       |
| `REDIS_REPLY_VERB`   | 14 | 带类型字符串（如 `SET` 的值）|

#### RedisResp::getType()

**函数：**

```cpp
int getType();
```

**描述：** 获取回复类型，返回值与上述 `REDIS_REPLY_*` 常量一致。转换前建议先检查类型，避免抛出异常。

#### RedisResp::asInt()

**函数：**

```cpp
long long asInt();
```

**描述：** 以整型读取回复。仅对 `REDIS_REPLY_INTEGER` 有效，其他类型抛出 `std::bad_cast`。

#### RedisResp::asDouble()

**函数：**

```cpp
double asDouble();
```

**描述：** 以浮点读取回复。支持 `REDIS_REPLY_DOUBLE` 与 `REDIS_REPLY_INTEGER`，其他类型抛出 `std::bad_cast`。

#### RedisResp::asString()

**函数：**

```cpp
std::string asString();
```

**描述：** 以字符串读取回复。支持 `REDIS_REPLY_ERROR`、`REDIS_REPLY_STRING`、`REDIS_REPLY_VERB`、`REDIS_REPLY_DOUBLE`、`REDIS_REPLY_STATUS`、`REDIS_REPLY_BIGNUM`，`REDIS_REPLY_INTEGER` 会转为十进制字符串。其他类型抛出 `std::bad_cast`。

#### RedisResp::asArray()

**函数：**

```cpp
const std::vector<RedisResp::ptr>& asArray();
```

**描述：** 以数组读取回复。将 `redisReply` 的每个元素包装为子 `RedisResp`（标记为 `is_child`，析构时**不释放**底层数据，由父对象统一释放）。结果被缓存，多次调用返回同一数组引用。仅对 `REDIS_REPLY_ARRAY` 有效，其他类型返回空数组。

**示例：**

```cpp
auto resp = co_await pool->executeQuery("LRANGE mylist 0 -1");
for(const auto& item : resp->asArray()) {
    std::cout << item->asString() << std::endl;
}
```

#### RedisResp::getState()

**函数：**

```cpp
IOState getState() const;
```

**描述：** 获取 IO 执行状态。`IOState::SUCCESS` 表示命令正常返回；`IOState::FAILED` 表示执行失败（如连接池关闭、命令执行出错返回 `nullptr`）。

---

### 共享基类 DBPool

​	`MySQLPoolManager` 与 `RedisPoolManager` 均继承自模板基类 `DBPool<ConnType, RespType>`。上文中 `borrowOneConn()`、`checkRunState()`、`registeConnCb()`、`tickle()`、`expand()` 等接口均由基类实现，两个连接池行为一致，此处不再重复。连接池的核心机制：

- **借出归还**：`ConnectWrapper` 包装器持有连接索引与连接池指针，析构时自动 `returnConn()` 归还连接。
- **扩容**：连接耗尽且未达 `max_conn` 时，`executeQuery()` 内部调用 `expand()` 按增量（`setIncreaseNum()`，默认 1）建立新连接。
- **等待**：达到 `max_conn` 后，`GetConnAwaiter` 通过 `registeConnCb()` 注册回调挂起协程，连接归还时 `tickle()` 唤醒。

## 架构概述

```
RedisPoolManager (继承 DBPool<RedisConn, RedisResp>)
  ├── m_connectors: vector<RedisConn::ptr>     // max_conn 个连接对象（redisContext*）
  ├── m_freeConnInfos: list<int>               // 空闲连接索引
  ├── m_waitConnCb: list<std::function<void()>>// 等待连接的回调队列
  └── m_state: INIT / READY / FULL / CLOSING / CLOSED / ERROR

executeQuery(cmd)
  → checkRunState()
  → borrowOneConn()           // 有空闲 -> 直接借出；无空闲 -> expand() 或 GetConnAwaiter 等待
  → RedisConn::executeQuery() // redisCommand() 同步执行，返回 redisReply* 封装为 RedisResp
  → wrapper.reset()           // 析构归还连接，tickle() 唤醒等待者
```

**与 MySQL 连接池的差异：**

| 维度           | MySQL                                     | Redis                                    |
| -------------- | ----------------------------------------- | ---------------------------------------- |
| 底层客户端     | MariaDB 非阻塞 C API                      | hiredis（同步接口）                      |
| IO 模型        | 异步：挂起协程监听 fd，不阻塞线程         | 阻塞：`redisCommand()` 直接同步等待响应   |
| 连接复用       | 需 `ROLLBACK` 回滚未提交事务              | 协议无状态，直接复用                     |
| 超时           | 有 `MYSQL_QUERY_TIMEOUT`（30s）           | 无内置超时                               |
| 结果封装       | `MySQLResp`（行/列两种访问）              | `RedisResp`（按类型转换）                |

## 错误

### 1. 阻塞命令阻塞调度线程

​	由于使用 hiredis 同步接口，`KEYS *`、`SMEMBERS`、大 `MGET` 等耗时命令会阻塞当前 IOManager 调度线程，期间该线程上的其他协程无法运行。建议：将重命令迁移到专门线程执行，或拆分命令避免单条命令耗时过长。

### 2. 连接池未初始化即使用

​	调用 `executeQuery()` 前必须成功调用 `init()`。与 MySQL 不同，Redis 连接池在 `INIT`（未初始化）或 `CLOSED` 状态下执行查询时，`checkRunState()` 返回 `false` 后代码会执行 `co_return std::make_shared<RedisResp>(nullptr, IOState::FAILED)`，而 `RedisResp` 构造函数对空指针触发 `assert` **直接中止进程**。因此务必先 `init()` 成功再执行命令。

### 3. 先析构 IOManager 再关闭连接池

​	与 MySQL 连接池一致，连接池依赖 IOManager 调度等待协程与归还回调。应遵循：先 `iom.autoStop()`（或析构 IOManager），再 `pool->close()`，避免连接在回调执行中被释放造成未知错误。

<...待完善>
