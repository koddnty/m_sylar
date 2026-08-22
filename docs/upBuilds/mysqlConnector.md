# MySQL 数据库连接池模块

## 模块介绍

​	MySQL 数据库连接池模块基于 MariaDB 提供的 [非阻塞客户端库](https://mariadb.com/docs/server/reference/product-development/mariadb-internals/using-mariadb-with-your-programs-api/non-blocking-client-library/non-blocking-api-reference)（`mysql_real_query_start`/`mysql_real_query_cont` 等系列接口）实现数据库的异步查询，通过 IOManager 监听 socket 可读/可写事件，再结合 C++20 协程封装为 `co_await` 接口，查询过程不会阻塞调度线程。

	连接池支持**普通文本 SQL 查询**与**预编译语句（Prepared Statement）**两种方式：

- **普通查询**：`executeQuery()` 提交一条 SQL，返回 `MySQLResp` 结果封装，提供逐行遍历与按列名随机访问两种读取方式。
- **预编译语句**：`MySQLStmt` 对 MariaDB stmt API 进行 C++ 风格封装，支持类型安全的参数绑定（`?` 占位符）与结果类型化取出，天然防止 SQL 注入，适合参数化查询与批量执行。

需要注意的是，本框架并未采用单例模式，由于数据库连接池依赖于 IOManager 调度，在使用前需初始化并绑定到具体的 IOManager 实例（与 TimeManager 类似）。一般而言，让用到连接池的任务都通过 IOManager 的 `schedule()` 方法调度即可。连接池模块关闭时也应该先析构依赖的 IOManager 模块再析构数据库连接池模块，否则可能会造成 IOManager 的对应 fd 异常关闭或无法正常关闭错误，详见[错误](#错误)。

## 快速开始

### 普通查询

```cpp
#include <iostream>
#include "DBPool/mysql.hpp"
#include "DBPool/factory.h"
#include "coroutine/corobase.h"

using namespace m_sylar;

// 使用 nextRow/nextValue 逐行遍历
Task<void, TaskBeginExecuter> testNext(MySQLPoolManager::ptr pool) {
    std::string sql = "select * from learn";
    MySQLResp::ptr resp = co_await pool->executeQuery(sql);     // 异步等待任务
    if(resp == nullptr) {
        std::cout << "failed to execute query" << std::endl;
        co_return;
    }
    auto row = resp->nextRow();
    while(row) {
        auto col = row.nextValue();
        while(col) {
            std::cout << col.get() << " ";
            col = row.nextValue();
        }
        row = resp->nextRow();
    }
    std::cout << std::endl;
}

// 调用 formatDate 后按列名随机访问
Task<void, TaskBeginExecuter> testMap(MySQLPoolManager::ptr pool) {
    std::string sql = "select * from learn limit 1";
    MySQLResp::ptr resp = co_await pool->executeQuery(sql);     // 异步等待任务
    if(resp == nullptr || resp->getState() != IOState::SUCCESS) {
        std::cout << "query failed" << std::endl;
        co_return;
    }
    resp->formatDate();                                         // 格式化到 map，支持索引访问
    std::cout << (*resp)["name"][0] << std::endl;
}

int main(void) {
    MySQLPoolManager::ptr mysql_pool = DB::createMysqlPool(10, 15);      // 创建公共连接池
    if(-1 == mysql_pool->init("localhost", "koddnty", "73256", "KoddntyDB", 3306, 0)) {
        std::cout << "failed to init dbPool" << std::endl;
        return -1;
    }

    IOManager iom("test_dbPool", 1);                            // 所有查询任务需通过 iom 调度

    for(int i = 0; i < 30; i++) {
        iom.schedule(TaskCoro20::create_coro(std::bind(testMap, mysql_pool)));
    }

    sleep(1);                   // 等待协程任务执行完毕
    iom.autoStop();             // 先停止 IOManager
    mysql_pool->close();        // 再关闭连接池
    return 0;
}
```

### 预编译语句查询

```cpp
#include "DBPool/mysql.hpp"
#include "DBPool/factory.h"
#include "coroutine/corobase.h"

using namespace m_sylar;

Task<void, TaskBeginExecuter> testStmtText(MySQLPoolManager::ptr pool) {
    // 借出连接（析构时自动归还）
    auto connection = pool->borrowOneConn();
    const std::string sql = "select name from learn where telephone_num = ?";

    // 声明结果类型为定长文本 STMT_Text<255>
    MySQLStmt<STMT_Text<255>> stmt(connection);
    co_await stmt.co_execute<std::string>(sql, "99999999999"); // 绑定参数并执行
    co_await stmt.co_storeAll();                               // 结果集缓存到 stmt
    co_await stmt.co_fetchAll();                               // 全部取出到 m_result

    const auto text = std::get<0>(stmt.getResult().getAll()[0]);
    const std::string resp = TextToString(text);               // STMT_Text -> string
    std::cout << "result: " << resp << std::endl;

    co_await stmt.co_close();                                  // 关闭 stmt 并归还连接
    co_return;
}
```

## API参考

| 函数/接口名称                                                | 功能                             | 所属类           | 等级   |
| :----------------------------------------------------------- | -------------------------------- | ---------------- | ------ |
| [MySQLPoolManager(int min_conn, int max_conn)](#mysqlpoolmanager-构造函数) | 创建一个连接池                   | MySQLPoolManager | 用户   |
| [MySQLPoolManager::init(...)](#mysqlpoolmanagerinit)         | 初始化连接池，建立最小连接数     | MySQLPoolManager | 用户   |
| [MySQLPoolManager::close()](#mysqlpoolmanagerclose)          | 关闭数据库连接池                 | MySQLPoolManager | 用户   |
| [executeQuery(const std::string&)](#executequerystdstring)   | 异步执行一条 SQL 语句            | MySQLPoolManager | 用户   |
| [borrowOneConn()](#borrowoneconn)                            | 借出一条空闲连接                 | MySQLPoolManager | 开发者 |
| [checkRunState()](#checkrunstate)                            | 检查连接池运行状态               | MySQLPoolManager | 用户   |
| [registeConnCb(...)](#registeconncb)                         | 注册连接资源可用任务回调         | MySQLPoolManager | 开发者 |
| [tickle()](#tickle)                                          | 唤醒等待中的回调，与 registeConnCb 配合 | MySQLPoolManager | 开发者 |
| [expand()](#expand)                                          | 扩容连接池                       | MySQLPoolManager | 开发者 |
| [MySQLResp::nextRow()](#mysqlrespnextrow)                    | 获取下一行                       | MySQLResp        | 用户   |
| [MySQLResp::Row::nextValue()](#rownextvalue)                 | 获取当前行的下一个字段           | MySQLResp::Row   | 用户   |
| [MySQLResp::resetRow()](#mysqlrespresetrow)                  | 将行游标重置到开头               | MySQLResp        | 用户   |
| [MySQLResp::formatDate()](#mysqlrespformatdate)              | 格式化数据到 map 支持列名索引    | MySQLResp        | 用户   |
| [MySQLResp::operator[](std::string)](#mysqlrespoperatorstdstring) | 按列名随机访问数据         | MySQLResp        | 用户   |
| [MySQLResp::co_fetchAll()](#mysqlrespco_fetchall)            | 异步拉取全部结果集到内存         | MySQLResp        | 开发者 |
| [MySQLResp::getColCount()](#mysqlrespgetcolcount--getrowcount) | 获取结果集列数                 | MySQLResp        | 用户   |
| [MySQLResp::getRowCount()](#mysqlrespgetcolcount--getrowcount) | 获取结果集行数                 | MySQLResp        | 用户   |
| [MySQLResp::getState()](#mysqlrespgetstate)                  | 获取 IO 执行状态                 | MySQLResp        | 用户   |
| [MySQLStmt<Cols...>(...)](#mysqlstmt-构造函数)                | 创建预编译语句对象               | MySQLStmt        | 用户   |
| [co_execute(query, params...)](#co_executequery-params)      | 绑定参数并执行预编译语句         | MySQLStmt        | 用户   |
| [co_storeAll()](#co_storeall)                                | 将结果集缓存到客户端             | MySQLStmt        | 用户   |
| [co_fetchNext()](#co_fetchnext)                              | 取出下一行结果                   | MySQLStmt        | 用户   |
| [co_fetchAll()](#co_fetchall)                                | 取出全部结果到 m_result          | MySQLStmt        | 用户   |
| [co_close()](#co_close)                                      | 关闭 stmt 并归还连接             | MySQLStmt        | 用户   |
| [getResult()](#getresult)                                    | 获取结果集引用                   | MySQLStmt        | 用户   |
| [makeParams(...)](#makeparams)                               | 便捷构造 StmtParams              | 全局             | 用户   |

---

### 工厂方法 DB

	连接池可通过 `DBPool/factory.h` 提供的工厂方法创建，支持公共实例（单例）与私有实例两种管理方式。

#### DB::createMysqlPool(min_conn, max_conn, is_public)

**函数：**

```cpp
static MySQLPoolManager::ptr createMysqlPool(int min_conn, int max_conn, bool is_public = true);
```

**参数：** `min_conn` 最小连接数量；`max_conn` 最大连接数量；`is_public` 为 `true` 时注册为公共实例，可通过 `DB::Mysql::getInstance()` 全局获取，为 `false` 时返回私有实例需自行管理。

**描述：** 创建一个 MySQL 连接池。**注意：工厂方法仅创建连接池对象，仍需手动调用 `init()` 完成连接。**

#### DB::Mysql::getInstance()

**函数：**

```cpp
static MySQLPoolManager::ptr getInstance();
```

**描述：** 获取公共 MySQL 连接池实例。若尚未创建，自动以 `(5, 15)` 参数创建（**仍未初始化**，需手动 `init()`）。

---

### MySQLPoolManager

#### MySQLPoolManager 构造函数

**函数：**

```cpp
MySQLPoolManager(int min_conn, int max_conn);
```

**参数：** `min_conn` 最小连接数量；`max_conn` 最大连接数量。

**描述：** 构造函数会一次性初始化所有（`max_conn`）数量的 `MySQLConn` 对象，但仅连接 `min_conn` 数量的连接。请确保 `0 <= min_conn <= max_conn`，否则抛出 `std::runtime_error`。

#### MySQLPoolManager::init(...)

**函数：**

```cpp
int init(const std::string& host,
         const std::string& user,
         const std::string& passwd,
         const std::string& db,
         unsigned int port,
         unsigned long client_flag);
```

**参数：** `host` 数据库地址，`user` 数据库用户名，`passwd` 数据库用户密码，`db` 数据库名，`port` 数据库端口，`client_flag` 一般设置为 0，详见 MariaDB C API 的 [mysql_real_connect](https://mariadb.com/docs/connectors/mariadb-connector-c/api-functions/mysql_real_connect) 函数。

**描述：** 进行实际的数据库连接。连接失败返回 `-1` 并将状态置为 `ERROR`，同时打印错误日志；成功返回 `0`。注意：调用 `init()` 前连接池状态需为 `INIT`，否则会直接置为 `ERROR`。

#### MySQLPoolManager::close()

**函数：**

```cpp
void close();
```

**描述：** 将连接池状态置为 `CLOSING` 并唤醒所有等待回调，等待中的协程会以失败结果返回。由于 MariaDB 非阻塞 API 自行管理 socket fd，与框架的 FdManager 存在冲突，请**先停止 IOManager 再调用 `close()`**（详见[错误](#错误)），否则可能造成 fd 异常关闭等未知错误。

#### executeQuery(const std::string&)

**函数：**

```cpp
Task<MySQLResp::ptr> executeQuery(const std::string& query);
```

**参数：** `query` 将要执行的 SQL 语句。

**描述：** 异步执行一条 SQL 语句，使用 `co_await` 获取返回值。线程安全。执行流程为：借出一条空闲连接 → 执行查询 → 异步拉取全部结果集到内存 → 执行 `ROLLBACK` 回滚未提交事务 → 归还连接。

**返回值：** 执行成功返回数据封装 `MySQLResp::ptr`；失败（如空 IOManager、关闭的连接池、SQL 错误、超时）返回状态非 `IOState::SUCCESS` 的响应对象（SQL 错误时仍可检查 `getState()`）。

#### borrowOneConn()

**函数：**

```cpp
virtual ConnectWrapper<MySQLConn, MySQLResp>::ptr borrowOneConn();
```

**描述：** 借出一条空闲连接，返回连接包装器。包装器析构时自动将连接归还连接池（`returnConn`）。连接池忙或状态非法时返回 `conn_idx == -1` 的空包装器。**线程不安全**，请仅在协程内使用。

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

#### expand()

**函数：**

```cpp
virtual int expand();
```

**描述：** 线程安全函数。按 `setIncreaseNum()` 设定的增量（默认 1）扩容连接池，新增连接立即建立并加入空闲列表。当达到 `max_conn` 时返回 `0` 不再扩容。

**返回值：** `0` 成功或已到上限；`-1` 扩容失败（新连接建立失败）。

---

### MySQLResp

**描述：** 查询结果封装。内部持有 `MYSQL_RES*`，析构时自动释放。提供两种数据访问方式：逐行遍历（`nextRow`/`nextValue`）与按列名随机访问（`formatDate` + `operator[]`）。

#### MySQLResp::nextRow()

**函数：**

```cpp
Row nextRow();
```

**描述：** 获取结果集中的下一行。返回的 `Row` 可隐式转换为 `bool` 判断是否有效（`m_row != nullptr`）。遍历到末尾时返回无效行，`while(row)` 循环结束。

#### Row::nextValue()

**函数：**

```cpp
Value nextValue();
```

**描述：** 获取当前行的下一个字段值。返回的 `Value` 可隐式转换为 `bool` 判断是否有效。`Value::get()` 返回 `std::string`，`Value::get(size_t&)` 返回原始 `char*` 指针与长度。

#### MySQLResp::resetRow()

**函数：**

```cpp
void resetRow();
```

**描述：** 将行游标重置到开头（`mysql_data_seek(resp, 0)`），可重复遍历结果集。

#### MySQLResp::formatDate()

**函数：**

```cpp
int formatDate();
```

**描述：** 将结果数据格式化到内部 `m_respMapData`（按列名索引），之后可通过 `operator[]` 随机访问。注意该操作有额外存储开销，且仅对 `SUCCESS` 状态的结果集有效。

**返回值：** `0` 成功；`-1` 失败（状态非法或无结果集）。

#### MySQLResp::operator[](std::string)

**函数：**

```cpp
ColProxy operator[](std::string fieldName);
```

**描述：** 按列名返回 `ColProxy` 代理对象，随后通过 `proxy[row_idx]` 取第 `row_idx` 行的字符串值。**需先调用 `formatDate()`**，否则列不存在时返回无效代理并打印错误日志。

**示例：**

```cpp
resp->formatDate();
std::cout << (*resp)["name"][0] << std::endl;   // 第一行的 name 列
```

#### MySQLResp::co_fetchAll()

**函数：**

```cpp
Task<IOState> co_fetchAll();
```

**描述：** 异步拉取全部结果集到内存（`mysql_store_result_start/cont`）。对无结果集的语句（如 `UPDATE`）返回 `SUCCESS` 且行列数为 0；查询超时返回 `IOState::TIMEOUT`。普通 `executeQuery()` 内部已调用，**仅单独使用 `MySQLConn` 时需手动调用**。

#### MySQLResp::getColCount() / getRowCount()

**函数：**

```cpp
[[nodiscard]] int getColCount() const;
[[nodiscard]] int getRowCount() const;
```

**描述：** 获取结果集的列数与行数。需在 `co_fetchAll()` 成功（`SUCCESS`）后读取；无结果集的语句（如 `UPDATE`）二者均为 0。

#### MySQLResp::getState()

**函数：**

```cpp
[[nodiscard]] IOState getState() const;
```

**描述：** 获取 IO 执行状态。`IOState::SUCCESS` 表示查询成功；`IOState::FAILED` 表示 SQL 执行出错；`IOState::TIMEOUT` 表示查询超时。

---

### MySQLStmt

#### MySQLStmt 构造函数

**函数：**

```cpp
template<typename... ResultType>
MySQLStmt(std::shared_ptr<ConnectWrapper<MySQLConn, MySQLResp>> conn_wrapper);
```

**参数：** `conn_wrapper` 通过 `pool->borrowOneConn()` 借出的连接包装器。

**描述：** 对 MariaDB stmt 查询进行 C++ 风格封装。模板参数 `ResultType...` 声明**结果列的类型**，支持 `STMT_Text<N>`、`STMT_INTEGER`、`STMT_FLOAT`、`STMT_BOOL`、`STMT_NULL`。构造时内部调用 `mysql_stmt_init` 初始化语句句柄。

#### co_execute(query, params...)

**函数：**

```cpp
template<typename... ParamType>
Task<IOState> co_execute(const std::string& query, ParamType&&... params);
```

**参数：** `query` 含 `?` 占位符的 SQL 语句；`params...` 绑定的参数值，支持 `std::string`、整型、浮点型、`bool` 及 `std::optional<T>`（`nullopt` 自动绑定为 SQL `NULL`）。

**描述：** 依次执行 prepare → bind param → bind result → execute。查询结束后状态转为 `EXECUTE`。

**返回值：** `IOState::SUCCESS` 成功；否则 `IOState::FAILED` 并打印错误日志。

#### co_storeAll()

**函数：**

```cpp
Task<IOState> co_storeAll();
```

**描述：** 将结果集从服务端缓存到客户端（`mysql_stmt_store_result`），之后才可逐行取出。执行前状态需为 `EXECUTE`。

**返回值：** `IOState::SUCCESS` 成功；否则 `IOState::FAILED`。

#### co_fetchNext()

**函数：**

```cpp
Task<std::optional<std::tuple<ResultType...>>> co_fetchNext();
```

**描述：** 取出下一行结果，返回 `std::optional<std::tuple<ResultType...>>`。取完所有行（`MYSQL_NO_DATA`）返回 `std::nullopt`。也可配合 `co_storeAll()` 后使用。

#### co_fetchAll()

**函数：**

```cpp
Task<IOState> co_fetchAll();
```

**描述：** 循环调用 `co_fetchNext()` 将全部行追加到 `m_result`（`StmtResult<ResultType...>`），之后通过 `getResult()` 访问。

#### co_close()

**函数：**

```cpp
Task<IOState> co_close();
```

**描述：** 关闭 stmt 句柄，并**归还连接**（重置 `m_conn_wrapper`，析构时自动 `returnConn`）。使用完毕后务必调用，否则连接一直处于借出状态。

#### getResult()

**函数：**

```cpp
const StmtResult<ResultType...>& getResult() const;
```

**描述：** 获取结果集引用。`StmtResult` 支持 `getAll()`（返回 `vector<tuple<Cols...>>`）、`size()`、`get<C>(R)`（取第 R 行第 C 列）、`operator[]`。

**示例：**

```cpp
const auto& result = stmt.getResult();
for (auto& row : result.getAll()) {
    std::cout << std::get<0>(row).toString() << std::endl;   // STMT_Text 转 string
}
```

---

### STMT 类型与工具函数

| 类型                    | 描述                                                        |
| ----------------------- | ----------------------------------------------------------- |
| `STMT_Text<N>`          | 定长文本缓冲区。`toString()`/`view()` 取字符串；`StringToText<N>(str)` 由字符串构造。 |
| `STMT_INTEGER`          | 64 位整型（`long long`）。                                  |
| `STMT_FLOAT`            | 双精度浮点（`double`）。                                    |
| `STMT_BOOL`             | 布尔值（`bool`）。                                          |
| `STMT_NULL`             | 空标记，仅用于参数绑定 NULL。                               |
| `TextToString<N>(STMT_Text<N>)` | 将 `STMT_Text<N>` 转换为 `std::string`（`NULL` 时返回空串）。 |
| `makeParams(args...)`   | 便捷构造 `StmtParams`，内部按参数类型生成 `MYSQL_BIND` 数组。 |

## 架构概述

```
MySQLPoolManager (继承 DBPool<MySQLConn, MySQLResp>)
  ├── m_connectors: vector<MySQLConn::ptr>     // max_conn 个连接对象
  ├── m_freeConnInfos: list<int>               // 空闲连接索引
  ├── m_waitConnCb: list<std::function<void()>>// 等待连接的回调队列
  └── m_state: INIT / READY / FULL / CLOSING / CLOSED / ERROR

executeQuery(sql)
  → checkRunState()
  → borrowOneConn()           // 有空闲 -> 直接借出；无空闲 -> expand() 或 GetConnAwaiter 等待
  → MySQLConn::executeQuery() // mysql_real_query_start/cont + MysqlAwaiter 协程异步等待
  → MySQLResp::co_fetchAll()  // mysql_store_result_start/cont 拉取全部结果
  → ROLLBACK                  // 回滚未提交事务，保证连接复用安全
  → wrapper.reset()           // 析构归还连接，tickle() 唤醒等待者
```

**执行流程（协程视角）：**

```
co_await executeQuery(sql)
  → MysqlAwaiter::on_suspend()
       → TimeManager::addEventWithTimeout(fd, READ|WRITE, resume, timeout)
       → IO 事件到达 -> resume(SUCCESS) | 超时 -> resume(TIMEOUT)
  → mysql_real_query_cont(...) 继续查询
  → co_await resp->co_fetchAll() 拉取结果
  → 返回 MySQLResp::ptr
```

## 配置项

| 配置项                    | 默认值 | 描述                                   |
| ------------------------- | ------ | -------------------------------------- |
| `MYSQL_QUERY_TIMEOUT`     | 30000  | 单条语句查询超时时间（毫秒），在 `mysql.hpp` 中定义为宏。 |

## 错误

### 1. IOManager 管理 fd 异常关闭错误

	模块采用 MariaDB 提供的非阻塞 C API 实现异步查询，但这套 API 内部会自行管理 socket fd，与当前框架的 FdManager 管理 fd 逻辑冲突。因此为避免冲突，需要**先关闭 IOManager**，确保事件均无监听、无回调时，再关闭数据库连接池，以确保事件的完整执行。同理，析构顺序应为：先析构 IOManager，再析构 MySQLPoolManager。

### 2. 连接池未初始化即使用

	调用 `executeQuery()` 前必须成功调用 `init()`（状态为 `READY`/`FULL`）。若在 `INIT` 状态下直接查询，`checkRunState()` 返回 `false`，`executeQuery()` 返回状态为 `IOState::FAILED` 的空响应对象，且会打印错误日志。

### 3. 预编译语句未关闭导致连接泄漏

	`MySQLStmt` 借出的连接在 `co_close()` 后才会归还连接池。若中途退出协程而未调用 `co_close()`，连接将一直处于借出状态，连接池逐渐耗尽。建议在协程中使用 RAII 或确保所有分支均调用 `co_close()`。

<...待完善>
