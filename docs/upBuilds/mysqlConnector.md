# 数据库连接池模块

## 模块介绍

​	数据库连接池模块暂时仅支持mysql数据库的异步访问，采用mariaDB提供的[C API](https://mariadb.com/docs/server/reference/product-development/mariadb-internals/using-mariadb-with-your-programs-api/non-blocking-client-library/non-blocking-api-reference)实现数据库的异步查询，通过iomanager以及协程模块封装实现了对数据库内容的异步查询，不会阻塞查询线程。

​	需要注意的是，本框架并未采用单个线程池实例模式，由于数据库连接池依赖于iomanager调度，在使用前需初始化并指定需要使用的ioamanger实例对象（与timeManager类似），指定方法特殊，一般而言需要让用到连接池的任务都通过iomanager的shcedule方法调度即可。连接池模块关闭时也应该先析构依赖的iomanager模块再析构数据库连接池模块，否则可能会造成iomanager的对应fd异常关闭或无法正常关闭错误，详见[错误](#错误)。

## 快速开始

``` cpp	
#include <iostream>
#include "DBPool/mysql.h"
#include "coroutine/corobase.h"

m_sylar::MySQLPoolManager* mysql_pool_mgr = nullptr; 
std::atomic<int> count = 0;

// 使用next类进行遍历
m_sylar::Task<void, m_sylar::TaskBeginExecuter> testNext () {
    std::string sql = "select * from learn";
    m_sylar::MySQLResp::ptr resp = co_await mysql_pool_mgr->executeQuery(sql);		// 异步等待任务
    if(resp) {
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
    else {
        std::cout << "failed to execute query" << std::endl;
    }
}

// 调用formatData后进行随机访问
m_sylar::Task<void, m_sylar::TaskBeginExecuter> testMap() {
    std::string sql = "select * from learn";
    m_sylar::MySQLResp::ptr resp = co_await mysql_pool_mgr->executeQuery(sql); 		// 异步等待任务
    resp->formatDate();
    std::cout << (*resp)["telephone_num"][4] << std::endl;
}

int main(void) {
    m_sylar::MySQLPoolManager dbPool(10, 20);
    //dbPool.init("<地址>", "<用户名>", "<数据库密码>", "<数据库名称>", <端口>, 0))
    if(-1 == dbPool.init("localhost", "koddnty", "73256", "KoddntyDB", 3306, 0)) {
        std::cout << "failed to init dbPool" << std::endl;
    }
    else {
        std::cout << "init dpPool successed" << std::endl;
    }
    mysql_pool_mgr = &dbPool;

    m_sylar::IOManager iom("test_dbPool", 1);


    for(int i = 0; i < 30; i++) {
        // iom.schedule(m_sylar::TaskCoro20::create_coro(testNext));
        iom.schedule(m_sylar::TaskCoro20::create_coro(testMap));
    }

    sleep(1);       // 等待执行到一半的协程任务。
    iom.autoStop();
    dbPool.close();
    return 0;
}
```

## API参考

| 函数/接口名称                                                | 功能                                | 所属类           | 等级 |
| :----------------------------------------------------------- | ----------------------------------- | ---------------- | ---- |
| [MySQLPoolManager(int min_conn, int max_conn)](#MySQLPoolManager(int min_conn, int max_conn)) | 创建一个连接池并绑定到一个iomanager | MySQLPoolManager | 用户 |
| [MySQLPoolManager::init(...)](#MySQLPoolManager::init(...))  | 初始化一个连接池，设定其参数        | MySQLPoolManager | 用户 |
| [MySQLPoolManager::close()](#MySQLPoolManager::close())      | 关闭一个数据库连接池                | MySQLPoolManager | 用户 |
| [Task<MySQLResp::ptr > executeQuery(const std::string& query);](#Task<MySQLResp::ptr > executeQuery(const std::string& query);) | 异步执行一个sql语句。               | MySQLPoolManager | 用户 |
|                                                              |                                     |                  |      |



### MySQLPoolManager

####     MySQLPoolManager(int min_conn, int max_conn);

**参数：** min_conn最小连接数量, max_conn 最大连接数量

**描述:** 构造函数会一次初始化所有(max_conn)数量的对象但仅连接min_conn数量的连接，请确保参数符合逻辑。

#### MySQLPoolManager::init(...)

**函数：** 

```cpp
    int init(const std::string& host,
            const std::string& user,
            const std::string& passwd,
            const std::string& db,
            unsigned int port,
            unsigned long clientflag);
```

**参数：** host数据库地址，user数据库用户名，passwd数据库用户密码，db数据库， port数据库端口，clientflag一般设置为0,详见mariaDB C API 的[mysql_init](https://mariadb.com/docs/connectors/mariadb-connector-c/api-functions/mysql_init)函数。

**描述：** 进行实际的数据库连接，失败返回-1并打印错误日志,成功返回0。

#### MySQLPoolManager::close()

**描述：**当调用close函数时，最好确保iomanager内部没有使用它的回调或事件，框架不会对此做出检查，否则可能会造成一些未知错误，例如[错误1](#1.iomanager管理fd异常关闭错误：)。

#### Task<MySQLResp::ptr > executeQuery(const std::string& query);

**参数：**query：将要执行的sql语句。

**描述：**执行一个sql语句，请使用co_await获取其返回值。非阻塞，线程安全。

**返回值：**sql执行成功则会返回数据封装，详见其他同文档api,若失败(如空iomanager或关闭的连接池)返回nullptr。

<...待完善>

## 错误

### 1.iomanager管理fd异常关闭错误：

​	模块采用mariaDB提供的C API实现异步的查询，但这套API内部会自行管理socket fd导致与当前框架的fdManager管理fd逻辑冲突，因此为避免这一冲突，需要先关闭iomanager确保事件均无监听无回调时关闭数据库连接池以确保事件的完整执行。







