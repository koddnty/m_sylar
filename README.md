# m_sylar:c++协程库

sylar项目地址:[sylar服务器框架原地址](https://github.com/sylar-yin/sylar)

sylar视频地址:[[C++高级教程]从零开始开发服务器框架(sylar)](https://www.bilibili.com/video/BV184411s7qF)

## 项目介绍

此项目原本是个人的学习项目，3个月学习后基本实现了原sylar项目的功能。但个人觉得掌握并不透彻，遂决定在原有框架的基础上，将原ucontext实现的协程模块替换为cpp20提供的无栈协程，并基于此实现iomanager等模块的适配。

在后续的不断实现中，也发现了原sylar框架的许多不足之处，现在框架已经移除了ucontext(也就是原sylar框架使用的协程方法)及调度模块，只继续实现基于新的基于cpp20协程的模块，修复了许多原框架存在的内存泄漏、内存错误等问题同时也优化了部分模块如单锁的任务队列模块。同时感谢[Benny Huo](https://www.bennyhuo.com/book/cpp-coroutines/00-foreword.html)的协程技术博客，对项目协程模块提供了很大帮助。

现在框架具有一定的稳定性，详见[性能分析](./docs/performance/performance_analysis.md)。也已经支持了http协议的简单解析，后续准备不断实现更多功能以及对底层进行更多优化如实现异步日志等。同时如果有一起学习的小伙伴欢迎一起讨论。

## 快速开始

### 项目依赖

- yaml-cpp   yaml解析器(配置模块)

### 构建

在build目录执行：

``` shell	
cmake ..
make -j8
```



### 简单实例(http)

``` cpp
#include <cstdio>
#include <memory>
#include "basic/log.h"
#include "basic/address.h"
#include "http/httpServer.h"

static m_sylar::Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

void home_page(m_sylar::http::HttpSession::ptr session)
{
    session->getResponse()->setHeader("nihao", "110");
    std::string message = "hello world";
    session->getResponse()->setBody(message);
}

void rename_func(m_sylar::http::HttpSession::ptr session)
{
    session->getResponse()->setHeader("nihao", "110");
    std::string message = "没有改名卡口我";
    session->getResponse()->setBody(message);
}

void test_http_server(m_sylar::IOManager* iom)
{
    // 为一个httpServer绑定地址端口
    m_sylar::http::HttpServer::ptr server(new m_sylar::http::HttpServer(iom));
    m_sylar::Address::ptr addr = m_sylar::Address::LookupAnyIPAddress("0.0.0.0");
    std::dynamic_pointer_cast<m_sylar::IPv4Address>(addr)->setPort(8803);
    server->bind(addr);
    
    server->GET("/home", home_page);		// 配置serverlet路由

    server->POST("/home/rename", rename_func);
    server->start(5);		// 启动server,传参指定同一地址listen fd个数
    M_SYLAR_LOG_INFO(g_logger) << "All Gate have been registered, ip:0.0.0.0:8803";
    sleep(40);
    server->stop();
}

int main(void)
{
    m_sylar::IOManager iom("httpServer", 4);    
    test_http_server(&iom);
    iom.autoStop();
    return 0;
}
```



其他部分如协程任务创建、调度、定时器等内容详见[接口与文档](# 接口与文档)，或参考代码实例。

## 接口与文档

- 基础模块
    - [日志](./docs/basic_modules/log.md)
    - [配置模块](./docs/basic_modules/config.md)
    - [协程任务](./docs/cppcoro/task.md)
    - [定时器模块](./docs/basic_modules/timer.md)
- 底层模块
    - [协程任务模块Task](./docs/cppcoro/task.md)
    - [scheduler和iomanager](./docs/cppcoro/iomanager.md)
    - [定时器模块](./docs/basic_modules/timer.md)(已经是基础模块，上文已提到)
- 上层模块
    - [socket封装](./docs/socket/socket.md)
    - http相关
        - [httpServer/serverlet](./docs/http/httpServer.md)

