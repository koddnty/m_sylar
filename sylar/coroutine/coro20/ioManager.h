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

class IOState {
public:
    enum State {
        SUCCESS = 0,
        TIMEOUT = 1,
        FAILED = 2,
        UNKNOWN = 3,
        INIT = 4,
    };
};


class IOManager : public Scheduler
{
public:
    using ptr = std::shared_ptr<IOManager>;
    IOManager(const std::string& name, int thread_num = 1);
    ~IOManager();

public:

    IOManager& addEvent(int fd, FdContext::Event event, TaskCoro20&& task);
    IOManager& addEvent(int fd, FdContext::Event event, std::function<void()> cb_func);
    IOManager& addOnceEvent(int fd, FdContext::Event event, TaskCoro20&& task);       // 只会单次执行回调的事件
    IOManager& addOnceEvent(int fd, FdContext::Event event, std::function<void()> cb_func);       // 只会单次执行回调的事件
    IOManager& delEvent(int fd, FdContext::Event event);
    IOManager& closeFd(int fd);
    IOManager& closeWithNoClose(int fd);            // (可能很奇怪，但没办法)关闭一个连接但不会close Fd， 用于第三方接口管理的fd处理。
    IOManager& cancelEvent(int fd, FdContext::Event event);
    IOManager& cancelAll(int fd);



    static IOManager* getInstance();
    void stateSync(FdContext::ptr fd_ctx, FdContext::Event origin_state);
    const FdContextManager::ptr getContext(int fd) {return m_fd_events[fd]; }


  
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