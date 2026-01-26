#include "iomanager.h"
#include "basic/hook.h"
#include "basic/log.h"
#include "basic/macro.h"
#include "lab/fiber.h"
#include "lab/schedule.h"
#include <cerrno>
#include <cinttypes>
#include <cstring>
#include <exception>
#include <functional>
#include <shared_mutex>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

namespace m_sylar
{


IOManagerCoro20::IOManagerCoro20(const std::string& name, int thread_num)
    : SchedulerCoro20(name, thread_num)
{
    // m_scheduler = SchedulerCoro20::GetThis();
    // M_SYLAR_ASSERT2(m_scheduler, "set scheduler failed");
    m_eventFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    m_epollFd = epoll_create(1);

    if(m_epollFd <= 0 || m_eventFd <= 0)
    {
        M_SYLAR_LOG_ERROR(g_logger) << "[iomanagerCoro20]failed to create eventfd or epollfd, errno=" << errno << " error:" << strerror(errno);
        return;
    }

    epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = m_eventFd;
    if(epoll_ctl(m_epollFd, EPOLL_CTL_ADD, m_eventFd, &event))
    {
        M_SYLAR_LOG_ERROR(g_logger) << "[iomanagerCoro20]failed to registe eventfd to epoll, errno=" << errno << " error:" << strerror(errno);
    }

    m_fd_contexts.resize(64);
    start();
}

IOManagerCoro20::~IOManagerCoro20()
{
    close(m_eventFd);
    close(m_epollFd);
}

void IOManagerCoro20::addEvent(int fd, Event event, TaskCoro20 task)
{
    bool is_have = false;
    Event total_event = event;
    // 检查原有事件
    if(m_fd_contexts[fd])
    {
        is_have = true;
        total_event = (Event)(total_event | m_fd_contexts[fd]->m_event);
    }
    
    // epoll 事件组装
    epoll_event new_event;
    new_event.events = total_event | EPOLLET;
    new_event.data.fd = fd;
    int rt = 0;
    if(is_have)
    {
        rt = epoll_ctl(m_epollFd, EPOLL_CTL_MOD, fd, &new_event);
    }
    else 
    {
        rt = epoll_ctl(m_epollFd, EPOLL_CTL_ADD, fd, &new_event);
    }
    if(rt)
    {   // 错误检查
        M_SYLAR_LOG_ERROR(g_logger) << "[iomanagerCoro20]faield to addEvent"
                                    << "errno=" << errno << " error:" << strerror(errno);
        return;
    }
    rt = 0;
    
    
    // fdcontex 设置
    FdContext::ptr fd_ctx(new FdContext(fd));
    fd_ctx->m_event = total_event;
    if(event & Event::READ)
    {
        fd_ctx->m_cb_read = task;
    }
    if(event & Event::WRITE)
    {
        fd_ctx->m_cb_write = task;
    }
    contextSet(fd, fd_ctx);
}

void IOManagerCoro20::delEvent(int fd, Event event)
{
    FdContext::ptr original_fd_ctx = m_fd_contexts[fd];
    if(!original_fd_ctx)
    {
        return;
    }
    // m_fd_contexts[fd] = event;
    Event total_event = (Event)(original_fd_ctx->m_event & ~event);
    
    // epoll 事件组装
    epoll_event new_event;
    new_event.events = total_event | EPOLLET;
    new_event.data.fd = fd;
    int rt = 0;
    if(total_event == Event::NONE)
    {
        rt = epoll_ctl(m_epollFd, EPOLL_CTL_DEL, fd, &new_event);
    }
    else
    {
        rt = epoll_ctl(m_epollFd, EPOLL_CTL_MOD, fd, &new_event);
    }
    if(rt)
    {   // 错误检查
        M_SYLAR_LOG_ERROR(g_logger) << "[iomanagerCoro20]faield to delEvent"
                                    << "errno=" << errno << " error:" << strerror(errno);
        return;
    }
    rt = 0;
    
    
    // fdcontex 修改
    if(total_event != Event::NONE)
    {
        m_fd_contexts[fd]->m_event = total_event;
        if(!(total_event & Event::READ))
        {
            m_fd_contexts[fd]->m_cb_read.reset();
        }
        if(!(total_event & Event::WRITE))
        {
            m_fd_contexts[fd]->m_cb_write.reset();
        }
    }
    else 
    {
        m_fd_contexts[fd] = nullptr;
    }
}

void IOManagerCoro20::cancelEvent(int fd, Event event)
{
    FdContext::ptr original_fd_ctx = m_fd_contexts[fd];
    if(!original_fd_ctx)
    {
        return;
    }
    // m_fd_contexts[fd] = event;
    Event total_event = (Event)(original_fd_ctx->m_event & ~event);
    
    // epoll 事件组装
    epoll_event new_event;
    new_event.events = total_event | EPOLLET;
    new_event.data.fd = fd;
    int rt = 0;
    if(total_event == Event::NONE)
    {
        rt = epoll_ctl(m_epollFd, EPOLL_CTL_DEL, fd, &new_event);
    }
    else
    {
        rt = epoll_ctl(m_epollFd, EPOLL_CTL_MOD, fd, &new_event);
    }
    if(rt)
    {   // 错误检查
        M_SYLAR_LOG_ERROR(g_logger) << "[iomanagerCoro20]faield to delEvent"
                                    << "errno=" << errno << " error:" << strerror(errno);
        return;
    }
    rt = 0;
    
    // 事件回调
    m_fd_contexts[fd]->trigger(event);

    // fdcontex 修改
    if(total_event != Event::NONE)
    {
        m_fd_contexts[fd]->m_event = total_event;
        if(!(total_event & Event::READ))
        {
            m_fd_contexts[fd]->m_cb_read.reset();
        }
        if(!(total_event & Event::WRITE))
        {
            m_fd_contexts[fd]->m_cb_write.reset();
        }
    }
    else 
    {
        m_fd_contexts[fd] = nullptr;
    }
}

void IOManagerCoro20::cancelAll(int fd)
{
    FdContext::ptr original_fd_ctx = m_fd_contexts[fd];
    if(!original_fd_ctx)
    {
        return;
    }
    // m_fd_contexts[fd] = event;
    
    // epoll 事件组装
    int rt = 0;
    rt = epoll_ctl(m_epollFd, EPOLL_CTL_DEL, fd, nullptr);
    if(rt)
    {   // 错误检查
        M_SYLAR_LOG_ERROR(g_logger) << "[iomanagerCoro20]faield to delEvent"
                                    << "errno=" << errno << " error:" << strerror(errno);
        return;
    }
    rt = 0;
    
    // 事件回调
    m_fd_contexts[fd]->trigger((Event)(Event::READ | Event::WRITE));

    // fdcontex 修改
    m_fd_contexts[fd] = nullptr;
}

void IOManagerCoro20::idle()
{

}

void IOManagerCoro20::tickle()
{

}


// void IOManagerCoro20::FdContext::reset()
// {

// }

void IOManagerCoro20::FdContext::trigger(Event event)
{
    if(event & Event::READ && m_cb_read.isLegal())
    {
        m_cb_read.resume();
    }
    if(event & Event::WRITE && m_cb_write.isLegal())
    {
        m_cb_write.resume();
    }
}

int IOManagerCoro20::contextSet(int fd, FdContext::ptr fd_ctx)
{
    std::unique_lock<std::shared_mutex> w_lock(m_mutex);
    if(fd > m_fd_contexts.size())
    {
        try
        {
            m_fd_contexts.resize(fd * 1.5);
        }
        catch(const std::exception& e)
        {
            M_SYLAR_LOG_ERROR(g_logger) << "[iomanagerCoro20]failed to resize m_fd_contexts"
                                        << "\noriginal size=" << m_fd_contexts.size()
                                        << "\non  set  size=" << (int)(fd * 1.5)
                                        << "\nerror" << e.what();
        }
    }
    m_fd_contexts[fd] = fd_ctx;
    return m_fd_contexts.size();
}



}