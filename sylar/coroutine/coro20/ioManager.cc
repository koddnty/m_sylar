#include "coroutine/coro20/ioManager.h"
#include "coroutine/coro20/hook.h"
#include "coroutine/coro20/fiber.h"
#include "coroutine/coro20/scheduler.h"
#include "basic/log.h"
#include "basic/macro.h"
#include "http/http.h"
#include <cerrno>
#include <cinttypes>
#include <cstdint>
#include <cstring>
#include <sys/param.h>
#include <exception>
#include <functional>
#include <shared_mutex>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

namespace m_sylar
{
static Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

IOManager::IOManager(const std::string& name, int thread_num)
    : Scheduler(name, thread_num)
{
    m_epollFd = epoll_create(1);
    if(m_epollFd == -1)
    {
        M_SYLAR_LOG_ERROR(g_logger) << "Failed to create a epoll fd, errno=" << errno << " error:" << strerror(errno);
        close(m_epollFd);
        throw;
        return;
    }
    m_eventFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if(m_eventFd == -1)
    {
        M_SYLAR_LOG_ERROR(g_logger) << "Failed to create a event fd, errno=" << errno << " error:" << strerror(errno);
        close(m_epollFd);
        close(m_eventFd);
        throw;
        return;
    }

    epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = m_eventFd;
    if(epoll_ctl(m_epollFd, EPOLL_CTL_ADD, m_eventFd, &event))
    {
        M_SYLAR_LOG_ERROR(g_logger) << "Failed to add eventfd to epoll, errno=" << errno << " error: "<< strerror(errno);
        close(m_epollFd);
        close(m_eventFd);
        return;
    }

    m_fd_events.resize(128);
    for(int i = 0; i < m_fd_events.size(); i++)
    {
        m_fd_events[i] = std::make_shared<FdContextManager>(i);
    }
    start();
}

IOManager::~IOManager()
{
    close(m_epollFd);
    close(m_eventFd);
}


IOManager& IOManager::addEvent(int fd, FdContext::Event event, TaskCoro20&& task)
{
    if(!task.isLegal())
    {
        M_SYLAR_LOG_WARN(g_logger) << "invalid task, task is illegal";
        return *this;
    }
    if(fd >= m_fd_events.size()) {ResizeEvents(fd); }
    // std::unique_lock<std::shared_mutex> wlock(m_mutex);
    std::shared_lock<std::shared_mutex> rlock(m_event_mutex);
    m_fd_events[fd]->addEvent(event, std::move(task),
        std::bind(&IOManager::stateSync, this, std::placeholders::_1, std::placeholders::_2));
    return *this;
}

IOManager& IOManager::addEvent(int fd, FdContext::Event event, std::function<void()> cb_func)
{
    if(!cb_func)
    {
        M_SYLAR_LOG_WARN(g_logger) << "invalid cb function, callback function is nullptr";
        return *this;
    }
    TaskCoro20 t {cb_func};
    if(fd >= m_fd_events.size()) {ResizeEvents(fd); }
    // std::unique_lock<std::shared_mutex> wlock(m_mutex);
    std::shared_lock<std::shared_mutex> rlock(m_event_mutex);
    m_fd_events[fd]->addEvent(event, std::move(t),
        std::bind(&IOManager::stateSync, this, std::placeholders::_1, std::placeholders::_2));
    return *this;
}

IOManager& IOManager::addOnceEvent(int fd, FdContext::Event event, TaskCoro20&& task)
{
    if(!task.isLegal())
    {
        M_SYLAR_LOG_WARN(g_logger) << "invalid task, task is illegal";
        return *this;
    }
    if(fd >= m_fd_events.size()) {ResizeEvents(fd); }
    // std::unique_lock<std::shared_mutex> wlock(m_mutex);
    std::shared_lock<std::shared_mutex> rlock(m_event_mutex);
    m_fd_events[fd]->addEvent(event, std::move(task),
        std::bind(&IOManager::stateSync, this, std::placeholders::_1, std::placeholders::_2));
    return *this;
}

IOManager& IOManager::addOnceEvent(int fd, FdContext::Event event, std::function<void()> cb_func)
{
    if(!cb_func)
    {
        M_SYLAR_LOG_WARN(g_logger) << "invalid cb function, callback function is nullptr";
        return *this;
    }
    TaskCoro20 t {cb_func};
    if(fd >= m_fd_events.size()) {ResizeEvents(fd); }
    // std::unique_lock<std::shared_mutex> wlock(m_mutex);
    std::shared_lock<std::shared_mutex> rlock(m_event_mutex);
    m_fd_events[fd]->addEvent(event, std::move(t),
        std::bind(&IOManager::stateSync, this, std::placeholders::_1, std::placeholders::_2));
    m_fd_events[fd]->delEvent(event, 
        std::bind(&IOManager::stateSync, this, std::placeholders::_1, std::placeholders::_2));
    return *this;
}


IOManager& IOManager::delEvent(int fd, FdContext::Event event)
{
    std::string s = "fd is bigger than fd_event.size(), id=" + std::to_string(fd) + "bad access" + std::to_string(m_fd_events.size());
    if(fd >= m_fd_events.size())
    {
        M_SYLAR_LOG_ERROR(g_logger) << s;
        throw std::runtime_error(s);
    }
    // M_SYLAR_ASSERT2(fd >= m_fd_events.size(), s.c_str());
    // std::unique_lock<std::shared_mutex> wlock(m_mutex);
    std::shared_lock<std::shared_mutex> rlock(m_event_mutex);

    m_fd_events[fd]->delEvent(event, std::bind(&IOManager::stateSync, this, std::placeholders::_1, std::placeholders::_2));
    return *this;
}

IOManager& IOManager::cancelEvent(int fd, FdContext::Event event)
{
    M_SYLAR_ASSERT2(fd < m_fd_events.size(), "fd is bigger than fd_event.size(), bad access");
    std::shared_lock<std::shared_mutex> rlock(m_event_mutex);
    m_fd_events[fd]->trigger(event, std::bind(&IOManager::stateSync, this, std::placeholders::_1, std::placeholders::_2));
    m_fd_events[fd]->delEvent(event, std::bind(&IOManager::stateSync, this, std::placeholders::_1, std::placeholders::_2));
    return *this;
}

IOManager& IOManager::cancelAll(int fd)
{
    FdContext::Event event = FdContext::Event(FdContext::Event::READ | FdContext::Event::WRITE);
    cancelEvent(fd, event);
    return *this;
}

IOManager* IOManager::getInstance()
{
    return dynamic_cast<IOManager*>(Scheduler::GetThis()); 
}

void IOManager::stateSync(FdContext::ptr fd_ctx, FdContext::Event origin_event)
{
    int epoll_ctl_state;
    FdContext::Event new_event = fd_ctx->getEvent();
    if(new_event == origin_event )
    {   // 无变化
        return;
    }
    else if(new_event == FdContext::NONE && origin_event != FdContext::NONE)
    {   // 删除事件
        epoll_ctl_state = EPOLL_CTL_DEL;
    }
    else if(origin_event == FdContext::NONE && new_event != FdContext::NONE) 
    {   // 添加事件
        epoll_ctl_state = EPOLL_CTL_ADD;
    }
    else
    {   // 修改事件
        epoll_ctl_state = EPOLL_CTL_MOD;
    }


    // epoll 事件组装
    epoll_event ep_event;
    ep_event.events = new_event | EPOLLET;
    ep_event.data.fd = fd_ctx->getFd();
    int rt = 0;

    rt = epoll_ctl(m_epollFd, epoll_ctl_state, fd_ctx->getFd(), &ep_event);



    if(epoll_ctl_state == EPOLL_CTL_DEL){
        M_SYLAR_LOG_DEBUG(g_logger) << "del " << fd_ctx->getFd();
    }

    if(rt == -1)
    {   // 错误检查
        M_SYLAR_LOG_ERROR(g_logger) << "[IOManager]faield to sync state"
                                    << "errno=" << errno << " error:" << strerror(errno);
    }
    return;
}

void IOManager::idle() 
{
    const static int MAX_EVENT_NUM = 4096;
    static const int MAX_TIMEOUT = 5000;

    // 等待epoll
    bool is_have_event = false;
    epoll_event* events = new epoll_event[MAX_EVENT_NUM];
    do
    {
        int rt = 0;
        m_idleThreadCount++;
        m_activeThreadCount--;
        do {
            rt = epoll_wait(m_epollFd, events, MAX_EVENT_NUM, MAX_TIMEOUT);

            if(rt == -1 && errno == EINTR)
            {
                continue;
            }
            else if(rt == -1)
            {
                M_SYLAR_LOG_ERROR(g_logger) << "[IOManager] faied to wait epoll fd:"
                                << "\n errno=" << errno << " error:" << strerror(errno);
                return;
            }
            else
            {
                break;
            }
        } while(!isStopping());
        m_idleThreadCount--;
        m_activeThreadCount++;
        
        // 任务处理
        for(int i = 0; i < rt; i++)
        {
            if(events[i].data.fd == m_eventFd)
            {   // 新任务fd
                uint64_t buffer = 0;
                while(read(m_eventFd, &buffer, sizeof(buffer)) > 0);
                is_have_event = true;
            }
            else
            {   // io 回调fd
                int io_fd = events[i].data.fd;
                std::shared_lock<std::shared_mutex> rlock(m_event_mutex);

                m_fd_events[io_fd]->trigger((FdContext::Event)events[i].events,
                    std::bind(&IOManager::stateSync, this, std::placeholders::_1, std::placeholders::_2));
            }
        }
    } while(!is_have_event);

    // 回退至running
    delete[] events;
}

void IOManager::tickle() 
{
//     int value = 1;
//     if(-1 == write(m_eventFd, &value, sizeof(value)))
//     {
//         M_SYLAR_LOG_ERROR(g_logger) << "Failed to write in m_eventFd, errno=" << errno << " error: " << strerror(errno);
//         throw;
    // }

    if(m_idleThreadCount > 0)
    {
        int64_t buffer = 1;
        if(1 == write(m_eventFd, &buffer, sizeof(buffer)))
        {
            M_SYLAR_LOG_ERROR(g_logger) << "[IOManager]failed to tickle othres while write eventFd"
                                        << "\nerrno=" << errno << " error:" << strerror(errno);
            throw;
            return;
        }
    }
}

void IOManager::ResizeEvents(int fd)
{
    std::unique_lock<std::shared_mutex> wlock(m_event_mutex);
    int curr_size = m_fd_events.size();
    if(fd >= curr_size)
    {   // 需扩容
        try
        {
            int after_size = MAX(curr_size * 1.5, fd * 1.3);
            m_fd_events.resize(after_size);
            for(int i = curr_size; i < after_size; ++i)
            {
                m_fd_events[i] = std::make_shared<FdContextManager>(i);
            }
        }
        catch(std::exception& e)
        {
            M_SYLAR_LOG_ERROR(g_logger) << "Failed to resize m_fd_events, error:" << e.what();
            throw ;
        }

    }
}

}