#pragma once
#include "log.h"
#include "config.h"
#include "macro.h"
#include "scheduler.h"
#include <atomic>
#include <cstddef>
#include <iostream>
#include <memory.h>
#include <mutex>
#include <shared_mutex>
#include <sys/epoll.h>
#include <vector>
#include <sys/eventfd.h>
#include <fcntl.h>

namespace m_sylar
{

class IOManager : public Scheduler 
{
public:
    using ptr = std::shared_ptr<IOManager>;

    IOManager(const std::string &name, size_t thread_num = 1, bool use_caller = false);
    ~IOManager();

        enum Event
        {
            NONE = 0x0,         // 无事件
            READ = EPOLLIN,     // 读事件
            WRITE = EPOLLOUT    // 写事件
        };

private:
    struct FdContext
    {
        struct EventContext 
        {
            Scheduler* scheduler = nullptr;               // 事件对应的调度器
            Fiber::ptr fiber;                   // 执行协程
            std::function<void()> cb_func = nullptr;      // 事件回调
        };

        EventContext& getEventContext(const Event& event);      // 获取一个FdContext对应的EventContext(read或者write)
        void resetEventContext(EventContext& event_ctx);      // 重置EventContext
        void trigger(const Event& event);

        EventContext read;          // 读事件
        EventContext write;         // 写事件
        int fd;                     // 事件关联句柄
        Event event = NONE;         // 已注册的事件
        std::shared_mutex rwmutex;
    };  

public:
    // 1 success 0 retry -1 failed
    int addEvent(int fd, Event event, std::function<void()> cb_func = nullptr);     // 添加事件

    bool delEvent(int fd, Event event);                                             // 删除事件
    
    bool cancelEvent(int fd, Event event);                                          // 取消事件

    bool cancelAll(int fd);                                                         // 取消全部事件

    size_t fdContextResize(size_t size);

    static IOManager* getThis();

protected:
    void tickle() override;

    bool stopping() override;

    void idle() override;

private:
    int m_epollFd;                                          // epoll文件描述符
    int m_eventFd;                                          // event通信句柄
    std::atomic<size_t> m_pendding_event_count;             // 事件计数
    std::shared_mutex m_rwmutex;
    std::vector<FdContext*> m_fdContexts;
};

}