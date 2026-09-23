#include <asm-generic/errno.h>
#include <sys/uio.h>
#include <asm-generic/socket.h>
#include <cerrno>
#include <cstdarg>
#include <cstdint>
#include <fcntl.h>      
#include <iostream>
#include <memory>
#include <ostream>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "basic/fdManager.h"
#include "coroutine/coro20/fiber.h"
#include "coroutine/coro20/hook.h"
#include "coroutine/coro20/ioManager.h"
#include "coroutine/coro20/task.hpp"
#include "basic/config.h"
#include "basic/log.h"
#include "basic/timer/timer.hpp"


namespace m_sylar
{
// #define HOOK_FUN(XX) \
//     XX(sleep) \
//     XX(usleep) \
//     XX(nanosleep) \
//     XX(socket) \
//     XX(accept) \
//     XX(connect) \
//     XX(read) \
//     XX(readv) \
//     XX(preadv) \
//     XX(preadv2) \
//     XX(recv) \
//     XX(recvfrom) \
//     XX(recvmsg) \
//     XX(write) \
//     XX(writev) \
//     XX(pwritev) \
//     XX(pwritev2) \
//     XX(send) \
//     XX(sendto) \
//     XX(sendmsg) \
//     XX(close) \
//     XX(fcntl) \
//     XX(ioctl) \
//     XX(getsockopt) \
//     XX(setsockopt) 

// static thread_local bool t_hook_enable = false;
static Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

static m_sylar::ConfigVar<uint64_t>::ptr g_tcp_connect_timeout =
    m_sylar::ConfigManager::LookUp("servers.http.tcpserver.timeout.connect", (uint64_t)5000, M_SYALR_LOG_KEY, "tcp connect timeout");     // 


/**
    @brief 用于同步io回调的超时和执行。

        所有状态必须遵从watting到finish或timo
*/
class fdTimerInfo
{
public:
    enum State 
    {
        UNKNOWN = 0,
        WATTING = 1,
        TIMEOUT = 2,
        FINISHED = 3
    };
    
    fdTimerInfo(fdTimerInfo& other) = delete;
    fdTimerInfo(fdTimerInfo&& other) = delete;
    fdTimerInfo() {}


    void init() 
    {
        std::unique_lock<std::mutex> lock {m_mutex};
        m_state = WATTING;
    }

    bool setTimo()
    {
        std::unique_lock<std::mutex> lock {m_mutex};
        if(m_state != WATTING) {return false;}
        m_state = TIMEOUT;
        return true;
    }

    bool setFinish()
    {
        std::unique_lock<std::mutex> lock {m_mutex};
        if(m_state != WATTING) {return false;}
        m_state = FINISHED;
        return true;
    }

    fdTimerInfo::State getState() 
    {
        std::unique_lock<std::mutex> lock {m_mutex};
        return m_state; 
    }

private:
    std::mutex m_mutex;
    State m_state = WATTING;
};


// template<typename Original_fun>

/**
    @brief 当前ioAwaiter仅会在有事件的时候唤醒协程，但不会进行读取或对事件处理

        co_await的结果为TimeLimitInfo::State:
            TimeLimitInfo::FINISHED 表示fd上的事件已就绪，调用方应重试原io函数
            TimeLimitInfo::TIMEOUT  表示等待超时，此时addEventWithTimeout已取消fd上的事件监听
 */
class io_Awaiter : public Awaiter<TimeLimitInfo::State> , public std::enable_shared_from_this<io_Awaiter>
{   // co_await do_io使用的Awiater,自动注册iomanager并在有信息时恢复协程。
public:
    io_Awaiter(int fd, m_sylar::FdContext::Event event, uint64_t timo_ms = 0)
        : m_fd(fd), m_timo(timo_ms), m_event(event) {}

    ~io_Awaiter() override
    {
        IOManager::getInstance()->delEvent(m_fd, m_event);
    }


    void on_suspend()override
    {
        // 超时时间单位ms, 未指定(0)时使用默认值
        uint64_t timo_ms = m_timo ? m_timo : HOOK_IOAWAIT_TIMEOUT;
        auto tim = m_sylar::TimeManager::getInstance();
        tim->addEventWithTimeout(m_fd, m_event, [this](){  // 回调函数，当有io事件可用或超时时恢复协程
            resume(*m_state);
        }, timo_ms, m_state);
    }

    void before_resume() override
    {
    }

private:
    // 出参,由addEventWithTimeout的条件定时器写入,值域见TimeLimitInfo::State
    TimeLimitInfo::StatePtr m_state = std::make_shared<TimeLimitInfo::State>(TimeLimitInfo::State::WAITING);
    int m_fd;
    uint64_t m_timo;        // ms
    m_sylar::FdContext::Event m_event;
};

template<typename Original_fun, typename ... Args>
static Task<int> do_io(int fd, Original_fun func, const char* fun_name,
    uint32_t event, int type, Args&& ... args)
{   // 文件描述符 原io函数 原函数名称 事件(读/写) 定时器任务类型(读/写) io函数其他参数
    // 对fd状态检查
    m_sylar::FdCtx::ptr fd_ctx = m_sylar::FdMgr::GetInstance()->get(fd, false);
    if(!fd_ctx)
    {
        // 非socket
        M_SYLAR_LOG_DEBUG(g_logger) << "fd_Ctx is null, not a socket, fd = " << fd;
        co_return func(fd, std::forward<Args>(args)...);
    }
    if (fd_ctx->is_closed())
    {
        errno = EBADFD;
        
        co_return -1;
    }
    if(!fd_ctx->is_socket() || fd_ctx->getUserNoblock())
    {
        co_return func(fd, std::forward<Args>(args) ...);
    }
    
    // 定时器设置: FdCtx中保存的为usec, 未设置时值为UINT64_MAX
    uint64_t time_us = fd_ctx->getTimeout(type);
    uint64_t time_out = (time_us == 0 || time_us == (uint64_t)-1)
                        ? HOOK_IOAWAIT_TIMEOUT                   // 未设置, 使用默认值(ms)
                        : (time_us + 999) / 1000;                // usec -> ms, 向上取整
    if(time_out == 0)
    {
        time_out = 1;
    }

retry:
    int n = func(fd, std::forward<Args>(args)...);
    while(n == -1 && errno == EINTR)
    {
        n = func(fd, std::forward<Args>(args)...);
    }

    if(n == -1 && errno == EAGAIN)
    {
        TimeLimitInfo::State time_limit = co_await io_Awaiter(fd, (m_sylar::FdContext::Event)event, time_out);      // 恢复时代表fd可进行event操作或者由于超时返回
        if (time_limit == TimeLimitInfo::FINISHED) {
            goto retry;                                         // fd事件已就绪, 重试原io函数
        }
        else if (time_limit == TimeLimitInfo::TIMEOUT) {
            // 超时, 此时addEventWithTimeout已取消fd上的事件监听
            M_SYLAR_LOG_DEBUG(g_logger) << fun_name << " timeout, fd = " << fd << " timeout = " << time_out << "ms";
            errno = ETIMEDOUT;
            co_return -1;
        }
        // 未知状态, 按失败处理
        errno = EIO;
        co_return -1;
    }

    co_return n;
}



// sleep 
class SleepAwaiter : public m_sylar::Awaiter<int>
{
public:
  SleepAwaiter(uint64_t time)
  {
    m_time = time;
  }

protected:
  void on_suspend() override
  {
    m_sylar::TimeManager::ptr tim = m_sylar::TimeManager::getInstance();
    TimeTask::ptr time_task = TimeTask::create(m_time, false, [this](TimeTask::ptr task)->Task<void> {
      resume(m_time);
      co_return ;
    });
    tim->addTimer(time_task);
  }

  void before_resume() override
  {
  }

private:
  uint64_t m_time;
};


m_sylar::Task<unsigned int> co_sleep(unsigned int seconds)          // msec
{
    co_await SleepAwaiter(seconds);
    co_return 0;
}

m_sylar::Task<int> co_usleep(useconds_t usec)
{
    co_await SleepAwaiter(usec);
    co_return 0;
}

m_sylar::Task<int> co_nanosleep(const struct timespec *duration,
                struct timespec* rem)
{
    uint64_t time = duration->tv_sec * 1000000 + (duration->tv_nsec / 1000);
    co_await SleepAwaiter(time);
    if(rem)
    {
    rem->tv_sec = 0;
    rem->tv_nsec = 0;
    }
    co_return 0; 
}

int co_socket(int domain, int type, int protocol)
{
    int fd = socket(domain, type, protocol);
    if(fd == -1)
    {
        return -1;
    }
    m_sylar::FdMgr::GetInstance()->get(fd, true);
    return fd;
}

m_sylar::Task<int> co_accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen)
{
    auto result = co_await do_io(sockfd, accept, "accept", m_sylar::FdContext::READ, SO_RCVTIMEO, addr, addrlen);
    if(result == -1)
    {
        // m_sylar::FdMgr::GetInstance()->del(sockfd);
        if(errno == ETIMEDOUT)
        {   // 监听fd空闲导致的超时, 由调用方重新accept, 不作为错误上报
            M_SYLAR_LOG_DEBUG(g_logger) << "accept timeout, no pending connection, sockfd=" << sockfd;
        }
        else
        {
            M_SYLAR_LOG_ERROR(g_logger) << "accept failed, sockfd=" << sockfd << " errno=" << errno;
        }
    }
    co_return result;
}

m_sylar::Task<int> co_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{   
    uint64_t connect_timo = m_sylar::g_tcp_connect_timeout->getValue();     // ms
    
    m_sylar::FdCtx::ptr fd_ctx = m_sylar::FdMgr::GetInstance()->get(sockfd, false);
    if(!fd_ctx || fd_ctx->is_closed() || !fd_ctx->is_init())
    {
        errno = EBADFD;
        co_return -1;
    }
    if(!fd_ctx->is_socket() || fd_ctx->getUserNoblock())        // 文件描述符非socket或用户需要非阻塞
    {   
        co_return connect(sockfd, addr, addrlen);
    }

    // 先直接尝试
    int n = connect(sockfd, addr, addrlen);
    if(n == 0)
    {
        co_return 0;
    }
    else if(errno != EINPROGRESS || n != -1)
    {
        co_return n;
    }

    // 使用定时器和iomanager进行connectfd的监听
    auto state = co_await m_sylar::io_Awaiter(sockfd, m_sylar::FdContext::WRITE, connect_timo);
    if(state == TimeLimitInfo::TIMEOUT)
    {
        M_SYLAR_LOG_INFO(g_logger) << "connect time out, sockfd:" << sockfd << " timo:" << connect_timo << "ms";
        errno = ETIMEDOUT;
        co_return -1;
    }
    else if(state != TimeLimitInfo::FINISHED)
    {
        M_SYLAR_LOG_ERROR(g_logger) << "connect failed, unexpected io await state, sockfd:" << sockfd
                                    << " state:" << (int)state;
        errno = EIO;
        co_return -1;
    }


    // 处理错误，分析返回值
    int error = 0;
    socklen_t len = sizeof(int);
    if(-1 == getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len))
    {
        co_return -1;
    }
    errno = error;
    if(!error)
    {
        co_return 0;
    }
    else
    {
        co_return -1;
    }
}

// read
m_sylar::Task<int> co_read(int fd, void* buf, size_t count)
{
    co_return co_await do_io(fd, read, "read", m_sylar::FdContext::READ, SO_RCVTIMEO,
                 buf, count);
}

m_sylar::Task<int > co_readv(int fd, const struct iovec *iov, int iovcnt)
{
    co_return co_await do_io(fd, readv, "readv", m_sylar::FdContext::READ, SO_RCVTIMEO,
                 iov, iovcnt);
}

m_sylar::Task<int > co_preadv(int fd, const struct iovec *iov, int iovcnt, off_t offset)
{
    co_return co_await do_io(fd, preadv, "preadv", m_sylar::FdContext::READ, SO_RCVTIMEO,
                 iov, iovcnt, offset);
}

m_sylar::Task<int> co_preadv2(int fd, const struct iovec *iov, int iovcnt, off_t offset, int flags)
{
    co_return co_await do_io(fd, preadv2, "preadv2", m_sylar::FdContext::READ, SO_RCVTIMEO,
                 iov, iovcnt, offset, flags);
}

m_sylar::Task<int> co_recv(int sockfd, void* buf, size_t len, int flags)
{
    co_return co_await do_io(sockfd, recv, "recv", m_sylar::FdContext::READ, SO_RCVTIMEO,
                buf, len, flags);
}

m_sylar::Task<int> co_recvfrom(int sockfd, void* buf, size_t len, int flags, struct sockaddr *  src_addr, socklen_t* addrlen)
{
    co_return co_await do_io(sockfd, recvfrom, "recvfrom", m_sylar::FdContext::READ, SO_RCVTIMEO,
                buf, len, flags, src_addr, addrlen);
}

m_sylar::Task<int> co_recvmsg(int sockfd, struct msghdr *msg, int flags)
{
    co_return co_await do_io(sockfd, recvmsg, "recvmsg", m_sylar::FdContext::READ, SO_RCVTIMEO,
                msg, flags);   
}


// write
m_sylar::Task<int> co_write(int fd, const void* buf, size_t count)
{
    co_return co_await do_io(fd, write, "write", m_sylar::FdContext::WRITE, SO_SNDTIMEO,
                buf, count); 
}

m_sylar::Task<int> co_writev(int fd, const struct iovec *iov, int iovcnt)
{
    co_return co_await do_io(fd, writev, "writev", m_sylar::FdContext::WRITE, SO_SNDTIMEO,
                iov, iovcnt); 
}
    
m_sylar::Task<int> co_pwritev(int fd, const struct iovec *iov, int iovcnt, off_t offset)
{
    co_return co_await do_io(fd, pwritev, "pwritev", m_sylar::FdContext::WRITE, SO_SNDTIMEO,
                iov, iovcnt, offset); 
}

m_sylar::Task<int> co_pwritev2(int fd, const struct iovec *iov, int iovcnt, off_t offset, int flags)
{
    co_return co_await do_io(fd, pwritev2, "pwritev2", m_sylar::FdContext::WRITE, SO_SNDTIMEO,
                iov, iovcnt, offset, flags); 
}

m_sylar::Task<int> co_send(int sockfd, const void* buf, size_t len, int flags)
{
    co_return co_await do_io(sockfd, send, "send", m_sylar::FdContext::WRITE, SO_SNDTIMEO,
                buf, len, flags); 
}
    
m_sylar::Task<int> co_sendto(int sockfd, const void* buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen)
{
    co_return co_await do_io(sockfd, sendto, "sendto", m_sylar::FdContext::WRITE, SO_SNDTIMEO,
                buf, len, flags, dest_addr, addrlen); 
}

m_sylar::Task<int> co_sendmsg(int sockfd, const struct msghdr *msg, int flags)
{
    co_return co_await do_io(sockfd, sendmsg, "sendmsg", m_sylar::FdContext::WRITE, SO_SNDTIMEO,
                msg, flags); 
}

// close, 一般mod不用设置，若为非0值则只进行epoll等清理不会closeFd.
int co_close(int fd, int mod){
    M_SYLAR_LOG_DEBUG(g_logger) << "co_close fd=" << fd << " in " << mod << " mod";
    if(mod == 0) {
        m_sylar::FdCtx::ptr fd_ctx = m_sylar::FdMgr::GetInstance()->get(fd);
        if(IOManager::getInstance())
        {
            // IOManager::getInstance()->closeFd(fd);
        }
        m_sylar::FdMgr::GetInstance()->del(fd);
        return close(fd);
    }
    else{
        m_sylar::FdCtx::ptr fd_ctx = m_sylar::FdMgr::GetInstance()->get(fd);
        if(IOManager::getInstance())
        {
            // IOManager::getInstance()->closeFd(fd);
        }
        m_sylar::FdMgr::GetInstance()->del(fd);
        return -1;
    }
    // return fd;
}

// functional       
int co_fcntl (int fd, int op, ...  )
{
    va_list ap;
    va_start(ap, op);

    switch(op)
    {
        case F_SETFL:
        {
            int arg = va_arg(ap, int);
            va_end(ap);

            m_sylar::FdCtx::ptr fd_ctx = m_sylar::FdMgr::GetInstance()->get(fd);
            if(!fd_ctx || fd_ctx->is_closed() || !fd_ctx->is_socket())
            {
                return fcntl(fd, op, arg);
            }
            fd_ctx->setUserNoblock(arg & O_NONBLOCK);
            if(fd_ctx->getSysNoblock())     // 保存系统原状态
            {
                arg |= O_NONBLOCK;                
            }
            else
            {
                arg &= !O_NONBLOCK;
            }
            return fcntl(fd, op, arg);
        }
            break;
        case F_GETFL:
            {
                va_end(ap);
                int value = fcntl(fd, op);
                // if(!m_sylar::is_hook_enable())
                // {
                //     return value;
                // }

                m_sylar::FdCtx::ptr fd_ctx = m_sylar::FdMgr::GetInstance()->get(fd, false);
                if(!fd_ctx || fd_ctx->is_closed() || !fd_ctx->is_socket())
                {
                    // 非socket
                    return value;
                } 
                if(fd_ctx->getUserNoblock())
                {
                    return value | O_NONBLOCK;     // 用户设置非阻塞
                }
                else
                {
                    // return value & ~O_NONBLOCK;    // 用户没有规定非阻塞，默认阻塞
                    return value & ~O_NONBLOCK ;    // 用户没有规定非阻塞，默认阻塞，是实际非阻塞
                }
            }
            break;

        case F_DUPFD:       // int
        case F_DUPFD_CLOEXEC:
        case F_SETFD:   
        case F_SETOWN:
        case F_SETSIG:
        case F_SETLEASE:
        case F_NOTIFY:
        case F_SETPIPE_SZ:
        case F_ADD_SEALS:
        {       
            int arg = va_arg(ap, int);
            va_end(ap);
            return fcntl(fd, op, arg);
        }
            break;

        // uint64-t*
        case F_GET_RW_HINT:
        case F_SET_RW_HINT:
        case F_GET_FILE_RW_HINT:
        case F_SET_FILE_RW_HINT:
        {
            uint64_t* arg = va_arg(ap, uint64_t*);
            va_end(ap);
            return fcntl(fd, op, arg);
        }
            break;
        // void
        // case F_GETFD:
        // case F_GETFL:        // 特殊
        // case F_GETOWN:
        // case F_GETSIG:
        // case F_GETLEASE:
        // case F_GETPIPE_SZ:
        // case F_GET_SEALS:
        // {

        // }
        //     break;

        // struct flock *
        case F_SETLK:
        case F_SETLKW :
        case F_GETLK:
        case F_OFD_SETLK:
        case F_OFD_SETLKW:
        case F_OFD_GETLK:
        {
            struct flock * arg = va_arg(ap, struct flock *);
            va_end(ap);
            return fcntl(fd, op, arg);
        }
            break;

        // struct f_owner_ex*
        case F_GETOWN_EX:
        case F_SETOWN_EX:
        {
            struct f_owner_ex* arg = va_arg(ap, struct f_owner_ex*);
            va_end(ap);
            return fcntl(fd, op, arg);
        }
            break;
        
        default:
            va_end(ap);
            return fcntl(fd, op);
    }
}

int co_ioctl(int fd, unsigned long op, ...)
{
    va_list ap;
    va_start(ap, op);
    void* arg = va_arg(ap, void*);
    va_end(ap);

    if(FIONBIO == op)
    {
        bool user_nonblock = !! *(int*)arg;
        m_sylar::FdCtx::ptr fd_ctx = m_sylar::FdMgr::GetInstance()->get(fd);
        if(!fd_ctx || fd_ctx->is_closed() || !fd_ctx->is_socket())
        {
            return fcntl(fd, op, arg);
        }
        fd_ctx->setUserNoblock(user_nonblock);
    } 
    return ioctl(fd, op, 1);
}


// option
int co_getsockopt(int sockfd, int level, int optname,
                    void* optval,
                    socklen_t * optlen)
{
    return getsockopt(sockfd, level, optname, optval, optlen);
}

int co_setsockopt(int sockfd, int level, int optname,
                    const void* optval,
                    socklen_t optlen)
{

    if(level == SOL_SOCKET)
    {
        if(optname == SO_RCVTIMEO || optname == SO_SNDTIMEO)
        {
            auto fd_ctx = m_sylar::FdMgr::GetInstance()->get(sockfd);
            if(fd_ctx)
            {
                const timeval* v = (const timeval*)optval;
                fd_ctx->setTimeout(optname, v->tv_sec * 1000000 + v->tv_usec);
            }
        }
    }
    return setsockopt(sockfd, level, optname, optval, optlen);
}

}

