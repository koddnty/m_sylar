#pragma once
#include "coroutine/coro20/fiber.h"
#include "coroutine/coro20/scheduler.h"
#include "coroutine/coro20/fdContext.h"
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
    using ptr = std::shared_ptr<IOManager>;
    IOManager(const std::string& name, int thread_num = 1);
    ~IOManager();

public:

    IOManager& addEvent(int fd, FdContext::Event event, TaskCoro20&& task);
    IOManager& addEvent(int fd, FdContext::Event event, std::function<void()> cb_func = nullptr);
    IOManager& delEvent(int fd, FdContext::Event event);
    IOManager& cancelEvent(int fd, FdContext::Event event);
    IOManager& cancelAll(int fd);

    static IOManager* getInstance();
    void stateSync(FdContext::ptr fd_ctx, FdContext::Event origin_state);

  
private:
    void idle() override;
    void tickle() override;
    void ResizeEvents(int fd);



    
private:
    int m_epollFd;      // epoll文件描述符
    int m_eventFd;      // event,事件描述符
    std::vector<FdContextManager::ptr> m_fd_events;  // 所有epoll管理的事件，与fd为一一对应关系
    std::shared_mutex m_event_mutex;        // 防止m_fd_events竞争锁
};

}