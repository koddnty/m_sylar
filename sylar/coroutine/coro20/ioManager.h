#pragma once
#include "coroutine/coro20/fiber.h"
#include "coroutine/coro20/scheduler.h"
#include "basic/fdManager.h"
#include "basic/log.h"
#include <cstdint>
#include <memory>
#include <sys/epoll.h>
#include <vector>

namespace m_sylar
{



class IOManager : public Scheduler
{
public:
    IOManager(const std::string& name, int thread_num = 1);
    ~IOManager();


public:
    void addEvent();
    void delEvent();

private:
    int m_epollFd;      // epoll文件描述符
    std::vector<FdContextManager> m_fd_events;  // 所有epoll管理的事件，与fd为一一对应关系
};

}