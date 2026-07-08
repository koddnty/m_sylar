# m_sylar:c++协程库

[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE) [![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](cpp)

sylar项目地址:[sylar服务器框架原地址](https://github.com/sylar-yin/sylar)

sylar视频地址:[[C++高级教程]从零开始开发服务器框架(sylar)](https://www.bilibili.com/video/BV184411s7qF)

## 项目介绍

此项目原本是个人的学习项目，3个月学习后基本实现了原sylar在B站发布的项目基础功能（到http模块）。但个人觉得掌握并不透彻，遂决定在原有框架的基础上，将原ucontext实现的协程模块替换为cpp20提供的无栈协程，并基于此实现iomanager等模块的适配。

在后续的不断实现中，也发现了先前实现学习过程中没注意到的许多问题，现在框架已经移除了ucontext(也就是原sylar框架使用的无栈协程)及调度模块(可从历史提交找回)，只继续实现基于新的基于cpp20协程的模块，修复了许多内存泄漏、内存错误等问题。同时也优化了部分模块如单锁的任务队列模块。感谢[Benny Huo](https://www.bennyhuo.com/book/cpp-coroutines/00-foreword.html)的协程技术博客，对项目协程模块提供了很大帮助。

现在框架具有一定的稳定性，详见[性能分析](./docs/performance/performance_analysis.md)。也已经支持了http协议的简单解析，后续准备不断实现更多功能以及对底层进行更多优化如实现异步日志等。同时如果有一起学习的小伙伴欢迎一起讨论。

## 项目状态

现已经实现了http的长连接支持，异步数据库连接池和redis连接池。正在实现基础的websocket支持，详见[TODO列表](docs/TODOS.md)

## 快速开始

### 项目依赖与构建

- boost库
- mariadb C connector
- hiredis(C)

``` shell
# 基础工具
sudo apt update
sudo apt install -y git cmake make g++

# boost库
sudo apt install -y libboost-dev

# mariaDB Connector C
git clone https://github.com/mariadb-corporation/mariadb-connector-c.git
cd mariadb-connector-c
mkdir build && cd build
cmake ..
make
sudo make install
cd ../..

# hiredis库
git clone https://github.com/redis/hiredis.git
cd hiredis
cmake .
make
sudo make install
cd ..

# 克隆项目
git clone https://github.com/koddnty/m_sylar.git
cd m_sylar
mkdir build
cd build
cmake ..
make
```

## 接口与文档

- 基础模块
  - [日志](./docs/basic_modules/log.md)
  - [配置模块](./docs/basic_modules/config.md)
  - [协程任务](./docs/cppcoro/task.md)
  - [定时器模块](./docs/basic_modules/timer.md)
- 底层模块
  - [协程任务模块Task](./docs/cppcoro/task.md)
  - [scheduler和iomanager](./docs/cppcoro/iomanager.md)
  - [定时器模块](./docs/basic_modules/timer.md)
- 上层模块
  - [socket封装](./docs/socket/socket.md)
  - http相关
    - [httpServer/serverlet](./docs/http/httpServer.md)
  - MySQL... 异步数据库连接池
  - Redis 同步数据库连接池

## 许可证

本项目采用 [MIT](LICENSE) 许可证。
