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
#include "basic/self_timer.h"


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
    m_sylar::configManager::Lookup("http.tcpserver.timeout.connect", (uint64_t)5000, "tcp connect timeout");     // 


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
    @brief 当前ioAwaiter仅会在有事件的时候唤醒协程，但不会进行读取或对事件处理，且返回值无效
 */
class io_Awaiter : public Awaiter<int> , public std::enable_shared_from_this<io_Awaiter>
{   // co_await do_io使用的Awiater,自动注册iomanager并在有信息时恢复协程。
    // 返回-1代表失败，-2代表应重试
public:
    // doIo_Awaiter(int fd, m_sylar::IOManager::Event event, std::function<void()> deal_func)
    //     : m_fd(fd), m_event(event), m_deal_func(deal_func) {}

    io_Awaiter(int fd, m_sylar::FdContext::Event event, uint64_t timo = -1)
        : m_fd(fd), m_timo(timo), m_event(event) {}

    ~io_Awaiter()
    {
        IOManager::getInstance()->delEvent(m_fd, m_event);
    }


    void on_suspend()override
    {
        // auto self = shared_from_this();
        m_sylar::IOManager* iom = m_sylar::IOManager::getInstance();
        std::shared_ptr<fdTimerInfo> fdtino (new fdTimerInfo);
        std::weak_ptr<fdTimerInfo> wfdtino (fdtino);
        
        // 回调事件注册
        iom->addEvent(m_fd, m_event, [this](){  // 回调函数，当有io事件可用或超时时恢复协程
            resume(m_state);
            // IOManager::getInstance()->delEvent(fd, event);
        });



        // 设置取消定时器
        // auto canceler = [wfdtino, weak_self](){
        //             // 设置取消fd
        //             auto self = weak_self.lock();
        //             if(!self) {return; }
        //             auto t = wfdtino.lock();
        //             if(t && t->setTimo())
        //             {   // 存活且成功设置超时
        //                 self->m_state = TIMO;    // 超时
        //                 IOManager::getInstance()->cancelEvent(self->m_fd, self->m_event);    // 恢复协程
        //             }
        //             // 不存活或超时设置失败
        //             return;
        //         };
        // if(m_timo != (uint64_t)-1){
        //     m_time_fd = tim->addConditionTimer(m_timo, false, [](){}, 
        //         [wfdtino](){
        //             if(wfdtino.lock() && wfdtino.lock()->getState() == fdTimerInfo::FINISHED)
        //             {
        //                 // 事件已执行
        //                 return true;
        //             }
        //             // 事件未执行
        //             return false;
        //         }, canceler 
        //     );
            
        // }
    }

    void before_resume() override
    {
        if(m_time_fd > 0)
        {   // 删除定时器
            m_sylar::TimeManager* tim = m_sylar::TimeManager::getInstance();
            tim->cancelTimer(m_time_fd);
        }
    }

    enum State
    {
        UNDEFINED = -2,
        ERROR = -1,
        READY = 0,
        TIMO = 1
    };

private:
    int m_state = READY;    // 0 事件可行， 1 超时， -1 出现错误 -2 未定义
    int m_fd;
    int m_time_fd = -1;
    uint64_t m_timo;
    m_sylar::FdContext::Event m_event;
};

template<typename Original_fun, typename ... Args>
static Task<ssize_t> do_io(int fd, Original_fun func, const char* fun_name, 
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
    
    // 定时器设置
    uint64_t time_out = fd_ctx->getTimeout(type);       // 获取设定的超时时间

retry:
    int n = func(fd, std::forward<Args>(args)...);
    while(n == -1 && errno == EINTR)
    {
        n = func(fd, std::forward<Args>(args)...);
    }

    if(n == -1 && errno == EAGAIN)
    {
        int rt = co_await io_Awaiter(fd, (m_sylar::FdContext::Event)event, time_out);      // 恢复时代表fd可进行event操作或者由于超时返回
        goto retry;
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
    m_sylar::TimeManager* tim = m_sylar::TimeManager::getInstance();
    tim->addTimer(m_time, false, [this](){
      resume(m_time);
    });
  }

  void before_resume() override
  {
  }

private:
  uint64_t m_time;
};


m_sylar::Task<unsigned int> co_sleep(unsigned int seconds)
{
    co_await SleepAwaiter(seconds * 1000000);
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

m_sylar::Task<ssize_t> co_accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen)
{
    auto result = co_await do_io(sockfd, accept, "accept", m_sylar::FdContext::READ, SO_RCVTIMEO, addr, addrlen);
    if(result == -1)
    {
        // m_sylar::FdMgr::GetInstance()->del(sockfd);
        M_SYLAR_LOG_ERROR(m_sylar::g_logger) << "accept failed, sockfd=" << sockfd;
    }
    co_return result;
}

m_sylar::Task<int> co_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{   
    uint64_t us_sleep = m_sylar::g_tcp_connect_timeout->getValue();
    
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
    auto state = co_await m_sylar::io_Awaiter(sockfd, m_sylar::FdContext::WRITE, us_sleep);
    if(state == io_Awaiter::TIMO)
    {
        M_SYLAR_LOG_INFO(g_logger) << "connect time out, sockfd:" << sockfd << " timo:" << us_sleep;
        errno = ETIMEDOUT;
        co_return -1;
    }
    else if(state != io_Awaiter::READY)
    {
        co_return connect(sockfd, addr, addrlen);
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
m_sylar::Task<ssize_t> co_read(int fd, void* buf, size_t count)
{
    co_return co_await do_io(fd, read, "read", m_sylar::FdContext::READ, SO_RCVTIMEO,
                 buf, count);
}

m_sylar::Task<ssize_t > co_readv(int fd, const struct iovec *iov, int iovcnt)
{
    co_return co_await do_io(fd, readv, "readv", m_sylar::FdContext::READ, SO_RCVTIMEO,
                 iov, iovcnt);
}

m_sylar::Task<ssize_t > co_preadv(int fd, const struct iovec *iov, int iovcnt, off_t offset)
{
    co_return co_await do_io(fd, preadv, "preadv", m_sylar::FdContext::READ, SO_RCVTIMEO,
                 iov, iovcnt, offset);
}

m_sylar::Task<ssize_t> co_preadv2(int fd, const struct iovec *iov, int iovcnt, off_t offset, int flags)
{
    co_return co_await do_io(fd, preadv2, "preadv2", m_sylar::FdContext::READ, SO_RCVTIMEO,
                 iov, iovcnt, offset, flags);
}

m_sylar::Task<ssize_t> co_recv(int sockfd, void* buf, size_t len, int flags)
{
    co_return co_await do_io(sockfd, recv, "recv", m_sylar::FdContext::READ, SO_RCVTIMEO,
                buf, len, flags);
}

m_sylar::Task<ssize_t> co_recvfrom(int sockfd, void* buf, size_t len, int flags, struct sockaddr *  src_addr, socklen_t* addrlen)
{
    co_return co_await do_io(sockfd, recvfrom, "recvfrom", m_sylar::FdContext::READ, SO_RCVTIMEO,
                buf, len, flags, src_addr, addrlen);
}

m_sylar::Task<ssize_t> co_recvmsg(int sockfd, struct msghdr *msg, int flags)
{
    co_return co_await do_io(sockfd, recvmsg, "recvmsg", m_sylar::FdContext::READ, SO_RCVTIMEO,
                msg, flags);   
}


// write
m_sylar::Task<ssize_t> co_write(int fd, const void* buf, size_t count)
{
    co_return co_await do_io(fd, write, "write", m_sylar::FdContext::WRITE, SO_SNDTIMEO,
                buf, count); 
}

m_sylar::Task<ssize_t> co_writev(int fd, const struct iovec *iov, int iovcnt)
{
    co_return co_await do_io(fd, writev, "writev", m_sylar::FdContext::WRITE, SO_SNDTIMEO,
                iov, iovcnt); 
}
    
m_sylar::Task<ssize_t> co_pwritev(int fd, const struct iovec *iov, int iovcnt, off_t offset)
{
    co_return co_await do_io(fd, pwritev, "pwritev", m_sylar::FdContext::WRITE, SO_SNDTIMEO,
                iov, iovcnt, offset); 
}

m_sylar::Task<ssize_t> co_pwritev2(int fd, const struct iovec *iov, int iovcnt, off_t offset, int flags)
{
    co_return co_await do_io(fd, pwritev2, "pwritev2", m_sylar::FdContext::WRITE, SO_SNDTIMEO,
                iov, iovcnt, offset, flags); 
}

m_sylar::Task<ssize_t> co_send(int sockfd, const void* buf, size_t len, int flags)
{
    co_return co_await do_io(sockfd, send, "send", m_sylar::FdContext::WRITE, SO_SNDTIMEO,
                buf, len, flags); 
}
    
m_sylar::Task<ssize_t> co_sendto(int sockfd, const void* buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen)
{
    co_return co_await do_io(sockfd, sendto, "sendto", m_sylar::FdContext::WRITE, SO_SNDTIMEO,
                buf, len, flags, dest_addr, addrlen); 
}

m_sylar::Task<ssize_t> co_sendmsg(int sockfd, const struct msghdr *msg, int flags)
{
    co_return co_await do_io(sockfd, sendmsg, "sendmsg", m_sylar::FdContext::WRITE, SO_SNDTIMEO,
                msg, flags); 
}

// close, 一般mod不用设置，若为非0值则只进行epoll等清理不会closeFd.
int co_close(int fd, int mod){

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

