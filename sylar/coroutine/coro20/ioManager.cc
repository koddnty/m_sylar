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
        return;
    }
    m_eventFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if(m_eventFd == -1)
    {
        M_SYLAR_LOG_ERROR(g_logger) << "Failed to create a event fd, errno=" << errno << " error:" << strerror(errno);
        close(m_epollFd);
        close(m_eventFd);
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

}


IOManager& IOManager::addEvent(int fd, FdContext::Event event, TaskCoro20 task)
{

}

IOManager& IOManager::addEvent(int fd, FdContext::Event event, std::function<void()> cb_func)
{
    TaskCoro20 t {cb_func};
    if(fd > m_fd_events.size()) {ResizeEvents(fd); }
    std::shared_lock<std::shared_mutex> rlock(m_mutex);
    m_fd_events[fd]->addEvent(event, std::move(t));
    return *this;
}

IOManager& IOManager::delEvent(int fd, FdContext::Event event)
{
    M_SYLAR_ASSERT2(fd < m_fd_events.size(), "fd is bigger than fd_event.size(), bad access");
    std::shared_lock<std::shared_mutex> rlock(m_mutex);
    m_fd_events[fd]->delEvent(event);
    return *this;
}

IOManager& IOManager::cancelEvent(int fd, FdContext::Event event)
{

}

IOManager& IOManager::cancelAll(int fd)
{

}

IOManager* IOManager::getInstance()
{

}

void IOManager::idle() 
{

}

void IOManager::tickle() 
{

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
            for(int i = curr_size; i < after_size; i++)
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