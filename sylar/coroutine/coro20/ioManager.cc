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
    // m_scheduler = Scheduler::GetThis();
    // M_SYLAR_ASSERT2(m_scheduler, "set scheduler failed");
    m_eventFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    m_epollFd = epoll_create(1);

    if(m_epollFd <= 0 || m_eventFd <= 0)
    {
        M_SYLAR_LOG_ERROR(g_logger) << "[IOManager]failed to create eventfd or epollfd, errno=" << errno << " error:" << strerror(errno);
        return;
    }

    epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = m_eventFd;
    if(epoll_ctl(m_epollFd, EPOLL_CTL_ADD, m_eventFd, &event))
    {
        M_SYLAR_LOG_ERROR(g_logger) << "[IOManager]failed to registe eventfd to epoll, errno=" << errno << " error:" << strerror(errno);
    }

    m_fd_contexts.resize(64);
    start();
}

IOManager::~IOManager()
{
    close(m_eventFd);
    close(m_epollFd);
}

int IOManager::addEvent(int fd, Event event, TaskCoro20 task)
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
        M_SYLAR_LOG_ERROR(g_logger) << "[IOManager]faield to addEvent"
                                    << "errno=" << errno << " error:" << strerror(errno);
        return -1;
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
    return 0;
}

int IOManager::addEvent(int fd, Event event, std::function<void()> cb_func)
{
    TaskCoro20 task(cb_func);
    return addEvent(fd, event, task);
}

bool IOManager::delEvent(int fd, Event event)
{
    FdContext::ptr original_fd_ctx = m_fd_contexts[fd];
    if(!original_fd_ctx)
    {
        return false;
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
        M_SYLAR_LOG_ERROR(g_logger) << "[IOManager]faield to delEvent"
                                    << "errno=" << errno << " error:" << strerror(errno);
        return false;
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
    return true;
}

bool IOManager::cancelEvent(int fd, Event event)
{
    FdContext::ptr original_fd_ctx = m_fd_contexts[fd];
    if(!original_fd_ctx)
    {
        return false;
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
        M_SYLAR_LOG_ERROR(g_logger) << "[IOManager]faield to delEvent"
                                    << "errno=" << errno << " error:" << strerror(errno);
        return false;
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

    return true;
}

bool IOManager::cancelAll(int fd)
{
    FdContext::ptr original_fd_ctx = m_fd_contexts[fd];
    if(!original_fd_ctx)
    {
        return false;
    }
    // m_fd_contexts[fd] = event;
    
    // epoll 事件组装
    int rt = 0;
    rt = epoll_ctl(m_epollFd, EPOLL_CTL_DEL, fd, nullptr);
    if(rt)
    {   // 错误检查
        M_SYLAR_LOG_ERROR(g_logger) << "[IOManager]faield to delEvent"
                                    << "errno=" << errno << " error:" << strerror(errno);
        return false;
    }
    rt = 0;
    
    // 事件回调
    m_fd_contexts[fd]->trigger((Event)(Event::READ | Event::WRITE));

    // fdcontex 修改
    m_fd_contexts[fd] = nullptr;
    return true;
}

void IOManager::idle()
{
    const static int MAX_EVENT_NUM = 64;
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
            {   // io 回调fd, 同时删除对应回调
                int io_fd = events[i].data.fd;
                
                m_fd_contexts[io_fd]->trigger((Event)events[i].events);

                // delEvent(io_fd, (Event)events[i].events);
            }
        }
    } while(!is_have_event);

    // 回退至running
    delete[] events;
}

void IOManager::tickle()
{
    if(m_idleThreadCount > 0)
    {
        int64_t buffer = 1;
        if(1 == write(m_eventFd, &buffer, sizeof(buffer)))
        {
            M_SYLAR_LOG_ERROR(g_logger) << "[IOManager]failed to tickle othres while write eventFd"
                                        << "\nerrno=" << errno << " error:" << strerror(errno);
            return;
        }
    }
}


// void IOManager::FdContext::reset()
// {

// }

IOManager::FdContext::FdContext(int fd)
{
    m_fd = fd;
    m_cb_read.reset();
    m_cb_write.reset();
    m_event = Event::NONE;
}

IOManager::FdContext::~FdContext()
{

}


void IOManager::FdContext::trigger(Event event)
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

int IOManager::contextSet(int fd, FdContext::ptr fd_ctx)
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
            M_SYLAR_LOG_ERROR(g_logger) << "[IOManager]failed to resize m_fd_contexts"
                                        << "\noriginal size=" << m_fd_contexts.size()
                                        << "\non  set  size=" << (int)(fd * 1.5)
                                        << "\nerror" << e.what();
        }
    }
    m_fd_contexts[fd] = fd_ctx;
    return m_fd_contexts.size();
}



}