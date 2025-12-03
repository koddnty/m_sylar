#include "ioManager.h"
#include "config.h" 
#include "fiber.h"
#include "log.h"
#include "macro.h"
#include "scheduler.h"
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/types.h>
#include <ucontext.h>
#include <unistd.h>
#include <vector>
#include <yaml-cpp/parser.h>

namespace m_sylar
{

static Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");   

IOManager::IOManager(const std::string &name, size_t thread_num, bool use_caller)
    : Scheduler(name, thread_num, use_caller)
{
    m_epollFd = epoll_create(1);
    M_SYLAR_ASSERT(m_epollFd > 0);
    
    m_eventFd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);

    epoll_event event;
    memset(&event, 0, sizeof(epoll_event));
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = m_eventFd;

    int rt = epoll_ctl(m_epollFd, EPOLL_CTL_ADD, m_eventFd, &event);
    M_SYLAR_ASSERT(rt == 0);

    M_SYLAR_LOG_DEBUG(g_logger) << "evnetFd = " << m_eventFd;
    fdContextResize(64);
    start();
}

IOManager::~IOManager()
{
    stop();
    close(m_epollFd);
    // M_SYLAR_LOG_DEBUG(g_logger) << "close eventFd";
    close(m_eventFd);
    for (size_t i = 0; i < m_fdContexts.size(); i++) {
        delete m_fdContexts[i];
    }
}

IOManager::FdContext::EventContext& IOManager::FdContext::getEventContext(const Event& event)
{
        switch (event) {
            case Event::READ :
                return read;
            case Event::WRITE :
                return write;
            default:
                M_SYLAR_ASSERT2(g_logger, 
                    "event : " + event + "  is illegal");
                throw std::invalid_argument("Invalid event type"); 
        }
}

void IOManager::FdContext::resetEventContext(EventContext& event_ctx)
{
    event_ctx.scheduler = nullptr;
    event_ctx.fiber.reset();
    event_ctx.cb_func = nullptr;
    return;
}

void IOManager::FdContext::trigger(const Event& event)
{
    M_SYLAR_ASSERT(this->event & event);
    this->event = (Event)(this->event & ~event);
    EventContext& context = getEventContext(event);
    M_SYLAR_ASSERT(!(context.cb_func && context.fiber));
    if(context.cb_func)
    {
        Scheduler::GetThis()->schedule(&context.cb_func);
    }
    else if(context.fiber) 
    {
        Scheduler::GetThis()->schedule(&context.fiber);
    }
    context.scheduler = nullptr;
    return;
}

// 0 success 1 retry -1 failed
int IOManager::addEvent(int fd, Event event, std::function<void()> cb_func)
{
    // 获取fd_ctx指针
    FdContext *fd_ctx = nullptr;
    {
        std::shared_lock<std::shared_mutex> w_lock(m_rwmutex);
        if (m_fdContexts.size() > (size_t)fd) {
          fd_ctx = m_fdContexts[fd];
        } else {
          fdContextResize(((m_fdContexts.size() * 2 > (size_t)fd) ? (m_fdContexts.size() * 2) : fd) );
          fd_ctx = m_fdContexts[fd];
        }
    }

    // 注册fd到epoll
    M_SYLAR_ASSERT(fd_ctx != nullptr);
    std::unique_lock<std::shared_mutex> w_lock(fd_ctx->rwmutex);
    if(fd_ctx->event & event)
    {
        // 事件重复
        M_SYLAR_LOG_ERROR(g_logger) << "fd = " << fd   
            << "  event = " << event 
            << "  fd_ctx->event = " << fd_ctx->event;
        M_SYLAR_ASSERT(!(fd_ctx->event & event));
    }

    int option = (fd_ctx->event == Event::NONE) ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;
    epoll_event epevent;
    epevent.events = EPOLLET | fd_ctx->event | event;
    epevent.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epollFd, option, fd, &epevent);
    if(rt == -1) 
    {
        // epoll_ctl失败
        M_SYLAR_LOG_ERROR(g_logger) << "fd:" << fd  
            << " event:" << event
            << " option: " << option
            << " rt: " << rt
            << " erron: " << strerror(errno);
        return -1;
    }
    ++m_pendding_event_count;

    // 设置EventContext
    fd_ctx->event = (Event)(fd_ctx->event | event);
    FdContext::EventContext& event_ctx = fd_ctx->getEventContext(event); 
    M_SYLAR_ASSERT(!(event_ctx.scheduler));
    M_SYLAR_ASSERT(!(event_ctx.fiber));
    M_SYLAR_ASSERT(!(event_ctx.cb_func));
    // M_SYLAR_ASSERT(!(event_ctx.scheduler || event_ctx.fiber || event_ctx.cb_func));

    event_ctx.scheduler = Scheduler::GetThis();
     
    if(cb_func)
    {
        // 回调函数任务
        event_ctx.cb_func.swap(cb_func);
    } else{
        // fiber协程任务
        event_ctx.fiber = Fiber::GetThisFiber();
        M_SYLAR_ASSERT(event_ctx.fiber->getState() == Fiber::EXEC);
    }
    return 0;
}

bool IOManager::delEvent(int fd, Event event)
{
    std::shared_lock<std::shared_mutex> r_lock (m_rwmutex);
    // 找到fd对应event事件
    if(m_fdContexts.size() < (size_t)fd) 
    {
        M_SYLAR_LOG_WARN(g_logger) << "fd index out of FdContexts range";
        return false;
    }

    FdContext* fd_ctx = m_fdContexts[fd];
    r_lock.unlock();

    // 检查事件关联性
    std::unique_lock<std::shared_mutex> w_lock (fd_ctx->rwmutex);
    if (!(fd_ctx->event & event)) {
        // 事件无关联
        return false;
    }

    // 为fd设置注册新的event
    Event new_event = (Event)(fd_ctx->event & ~event);
    int option = (new_event == 0) ? EPOLL_CTL_DEL : EPOLL_CTL_MOD;
    epoll_event ep_event;
    ep_event.events = option | EPOLLET;
    ep_event.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epollFd, option, fd, &ep_event);
    if(rt == -1) 
    {
        // epoll_ctl失败
        M_SYLAR_LOG_ERROR(g_logger) << "fd:" << fd  
            << " event:" << event
            << " option: " << option
            << " rt: " << rt
            << " erron: " << strerror(errno);
        return false;
    }
    --m_pendding_event_count;
    
    // 新事件复写回FdContext
    fd_ctx->event = new_event;
    FdContext::EventContext& event_ctx = fd_ctx->getEventContext(event);
    fd_ctx->resetEventContext(event_ctx);   
    return true;
}

bool IOManager::cancelEvent(int fd, Event event)
{
    std::shared_lock<std::shared_mutex> r_lock (m_rwmutex);
    // 找到fd对应event事件
    if(m_fdContexts.size() < (size_t)fd) 
    {
        M_SYLAR_LOG_WARN(g_logger) << "fd index out of FdContexts range";
        return false;
    }

    FdContext* fd_ctx = m_fdContexts[fd];
    r_lock.unlock();

    // 检查事件关联性
    std::unique_lock<std::shared_mutex> w_lock (fd_ctx->rwmutex);
    if (!(fd_ctx->event & event)) {
        // 事件无关联
        return false;
    }

    // 为fd设置注册新的event
    Event new_event = (Event)(fd_ctx->event & ~event);
    int option = (new_event == 0) ? EPOLL_CTL_DEL : EPOLL_CTL_MOD;
    epoll_event ep_event;
    ep_event.events = option | EPOLLET;
    ep_event.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epollFd, option, fd, &ep_event);
    if(rt == -1) 
    {
        // epoll_ctl失败
        M_SYLAR_LOG_ERROR(g_logger) << "fd:" << fd  
            << " event:" << event
            << " option: " << option
            << " rt: " << rt
            << " erron: " << strerror(errno);
        return false;
    }

    // 触发回调
    fd_ctx->trigger(event);
    --m_pendding_event_count;
    
    // 新事件复写回FdContext
    fd_ctx->event = new_event;
    FdContext::EventContext& event_ctx = fd_ctx->getEventContext(event);
    fd_ctx->resetEventContext(event_ctx);   
    return true;
}

bool IOManager::cancelAll(int fd)
{
    std::shared_lock<std::shared_mutex> r_lock (m_rwmutex);
    // 找到fd对应event事件
    if(m_fdContexts.size() < (size_t)fd) 
    {
        M_SYLAR_LOG_WARN(g_logger) << "fd index out of FdContexts range";
        return false;
    }

    FdContext* fd_ctx = m_fdContexts[fd];
    r_lock.unlock();

    // 检查事件关联性
    std::unique_lock<std::shared_mutex> w_lock (fd_ctx->rwmutex);
    if (!(fd_ctx->event)) {
        // 无事件
        return false;
    }

    // 为fd设置注册新的event
    int option = EPOLL_CTL_DEL;
    epoll_event ep_event;
    ep_event.events = option;
    ep_event.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epollFd, option, fd, &ep_event);
    if(rt == -1) 
    {
        // epoll_ctl失败
        M_SYLAR_LOG_ERROR(g_logger) << "fd:" << fd  
            << " event:" << ep_event.events
            << " option: " << option
            << " rt: " << rt
            << " erron: " << strerror(errno);
        return false;
    }

    // 触发回调 + 空事件复写回FdContext
    if(fd_ctx->event & READ)
    {
        fd_ctx->trigger(READ);
        --m_pendding_event_count;

        FdContext::EventContext& event_ctx = fd_ctx->getEventContext(READ);
        fd_ctx->resetEventContext(event_ctx);   
    }
    if(fd_ctx->event & WRITE)
    {
        fd_ctx->trigger(Event::WRITE);
        --m_pendding_event_count;

        FdContext::EventContext& event_ctx = fd_ctx->getEventContext(WRITE);
        fd_ctx->resetEventContext(event_ctx);   
    }

    M_SYLAR_ASSERT(fd_ctx->event == Event::NONE);        // 应当在trigger中实现
    return true;    
}

size_t IOManager::fdContextResize(size_t size)
{
    try 
    {
        m_fdContexts.resize(size, nullptr);    
    }
    catch(const std::exception& e)
    {
        M_SYLAR_LOG_ERROR(g_logger) << e.what();
    }
    
    // 添加fd_ctx内容
    for(size_t i = 0; i < m_fdContexts.size(); ++i)
    {
        if(m_fdContexts[i] == nullptr)
        {
            m_fdContexts[i] = new FdContext;
            m_fdContexts[i]->fd = i;
        }
    }
    return size;
}

IOManager* IOManager::getThis()
{
    return dynamic_cast<IOManager*>(Scheduler::GetThis());
}

void IOManager::tickle()
{
    if(!isHaveIdleThread())
    {
        return;
    }
    // M_SYLAR_LOG_INFO(g_logger) << "iomanager tickle()";
    int64_t buffer = 1;
    int rt = write(m_eventFd, &buffer, sizeof(int64_t));
    M_SYLAR_ASSERT(rt > 0);
}

bool IOManager::stopping()
{
    return Scheduler::stopping()
        && m_pendding_event_count == 0;
}

void IOManager::idle()
{
    const static int MAX_EVENT_NUM = 64;
    static const int MAX_TIMEOUT = 5000;

    epoll_event* ep_events = new epoll_event[MAX_EVENT_NUM];
    std::shared_ptr<epoll_event> shared_events (ep_events, [](epoll_event* ep_event){
        delete [] ep_event;
    });

    while(true) 
    {
        if(stopping())
        {
            M_SYLAR_LOG_INFO(g_logger) << "idle stopping";
            break;
        }

        // 持续监听io任务
        int rt = 0;
        do {
            rt = epoll_wait(m_epollFd, ep_events, MAX_EVENT_NUM, MAX_TIMEOUT);
            if (rt >= 0)
            {
                // M_SYLAR_LOG_DEBUG(g_logger) << "new event, rt=" << rt;
                break;
            }
            else if (errno == EINTR)
            {
                M_SYLAR_LOG_DEBUG(g_logger) << "thread is stopped, retry rt=" << rt;
                // 被信号中断, 重试
                continue;
            }
            else {
                // 错误
                M_SYLAR_LOG_ERROR(g_logger) << "epoll_wait failed, errno:" << errno << " error " << strerror(errno);
                break;
            }
        } while(true);

        // 处理简单io任务
        for(int i = 0; i < rt; i++)
        {
            epoll_event ep_event = ep_events[i];
            if(ep_event.data.fd == m_eventFd)               // eventFd 存储用的fd，任务存储用对应Fdcontext
            {
                // eventFd 数据，唤醒，无任务
                // M_SYLAR_LOG_DEBUG(g_logger) << "message form eventFd, new task";
                int64_t dummy = 0;
                while(read(m_eventFd, &dummy, sizeof(int64_t)) > 0) ;
                continue;
            }   

            FdContext* fd_ctx = (FdContext*)ep_event.data.ptr;
            std::unique_lock<std::shared_mutex> w_lock (fd_ctx->rwmutex);
            // if(ep_event.events & (EPOLLERR | EPOLLHUP))
            // {
                
            //     break;
            // }

            uint32_t real_event = Event::NONE;
            if(ep_event.events & EPOLLIN)
            {
                real_event |= Event::READ;
            }
            if(ep_event.events & EPOLLOUT)
            {
                real_event |= Event::WRITE;
            }

            if(!(real_event & fd_ctx->event))
            {
                // 空事件
                continue;
            }

            // 写回剩余事件
            // M_SYLAR_LOG_DEBUG(g_logger) << "epoll    task:" << real_event;
            // M_SYLAR_LOG_DEBUG(g_logger) << "registed task:" << fd_ctx->event;
            // M_SYLAR_LOG_DEBUG(g_logger) << "task fd:" << fd_ctx->fd;
            int left_event = fd_ctx->event & ~real_event;
            int op = (left_event == Event::NONE) ? EPOLL_CTL_DEL : EPOLL_CTL_MOD; 
            ep_event.events = EPOLLET | left_event;
            
            int rt = epoll_ctl(m_epollFd, op, fd_ctx->fd, &ep_event);
            if(rt)
            {
                M_SYLAR_LOG_ERROR(g_logger) << "epoll_ctl (" << m_epollFd << ", "
                    << op << ", " << fd_ctx->fd << ", " << ep_event.events << ") "
                    << "errno : " << strerror(errno);
                continue;
            }
            
            // 触发io任务回调事件
            if((real_event & fd_ctx->event) & Event::READ)
            {
                fd_ctx->trigger(Event::READ);
                --m_pendding_event_count;
            }
            if((real_event & fd_ctx->event) & Event::WRITE)
            {
                fd_ctx->trigger(Event::WRITE);
                --m_pendding_event_count; 
            }
        }

        // 回退至running， 下一轮任务检测
        Fiber::ptr cur = Fiber::GetThisFiber();
        auto raw_ptr = cur.get();
        cur.reset();            // 防止长时间持有，导致无法析构
        raw_ptr->swapOut();
    }

}

}