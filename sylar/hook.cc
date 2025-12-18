#include "hook.h"
#include <asm-generic/errno.h>
#include <asm-generic/socket.h>
#include <cerrno>
#include <cstdarg>
#include <cstdint>
#include <fcntl.h>
#include <memory>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "fdManager.h"
#include "ioManager.h"
#include "log.h"
#include "self_timer.h"


namespace m_sylar
{
#define HOOK_FUN(XX) \
    XX(sleep) \
    XX(usleep) \
    XX(nanosleep) \
    XX(socket) \
    XX(accept) \
    XX(connect) \
    XX(read) \
    XX(readv) \
    XX(preadv) \
    XX(preadv2) \
    XX(recv) \
    XX(recvfrom) \
    XX(recvmsg) \
    XX(write) \
    XX(writev) \
    XX(pwritev) \
    XX(pwritev2) \
    XX(send) \
    XX(sendto) \
    XX(sendmsg) \
    XX(close) \
    XX(fcntl) \
    XX(ioctl) \
    XX(getsockopt) \
    XX(setsockopt) 

static thread_local bool t_hook_enable = false;
static Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

struct fdTimerInfo
{
public:
    int is_cancelled = 0;
};

template<typename Original_fun, typename ... Args>
static ssize_t do_io(int fd, Original_fun func, const char* fun_name, 
    uint32_t event, int type, Args&& ... args)
{
    if(!m_sylar::is_hook_enable())
    {
        return func(fd, std::forward<Args>(args)...);
    }

    // 对fd状态检查
    m_sylar::FdCtx::ptr fd_ctx = m_sylar::FdMgr::GetInstance()->get(fd, false);
    if(!fd_ctx)
    {
        // 非socket
        return func(fd, std::forward<Args>(args)...);
    }
    if (fd_ctx->is_closed())
    {
        errno = EBADFD;
        return -1;
    }
    if(!fd_ctx->is_socket() || fd_ctx->getUserNoblock())
    {
        return func(fd, std::forward<Args>(args) ...);
    }
    
    // 定时器设置
    uint64_t time_out = fd_ctx->getTimeout(type);       // 获取设定的超时时间
    std::shared_ptr<fdTimerInfo> fdtino (new fdTimerInfo);

retry:
    int n = func(fd, std::forward<Args>(args)...);
    while(n == -1 && errno == EINTR)
    {
        n = func(fd, std::forward<Args>(args)...);
    }

    if(n == -1 && errno == EAGAIN)
    {
        // 当前无数据, 添加定时器,注册到iomanager
        m_sylar::IOManager* iom = m_sylar::IOManager::getInstance();
        m_sylar::TimeManager* tim = m_sylar::TimeManager::getInstance();
        std::weak_ptr<fdTimerInfo> wfdtino (fdtino);

        // 设置超时定时器
        int timer_fd = -1;
        if(time_out != (uint64_t)-1)
        {
            timer_fd = tim->addConditionTimer(time_out, false, [](){}, 
                [wfdtino](){
                    if(wfdtino.lock() && wfdtino.lock()->is_cancelled)
                    {
                        // 事件已执行
                        return true;
                    }
                    return false;
                }, 
                [fdtino, iom, event, fd](){
                    // 设置取消fd
                    fdtino->is_cancelled = ETIMEDOUT;
                    iom->cancelEvent(fd, (m_sylar::IOManager::Event)event);
                });
        }

        // 设置事件
        int rt = iom->addEvent(fd, (m_sylar::IOManager::Event)event);
        if(rt)
        {
            // 事件添加失败
            M_SYLAR_LOG_ERROR(g_logger) << "addEvent failed, return value=" << rt;
            if(timer_fd > 0)
            {
                tim->cancelTimer(timer_fd);
            }
        }
        else
        {
            m_sylar::Fiber::YieldToHold();      // 交出控制权，等待addEvent或者addConditionTimer唤醒
            if(fdtino->is_cancelled)
            {
                // 超时
                errno = fdtino->is_cancelled;
                return -1;
            }
            else
            {
                // 取消定时器
                tim->cancelTimer(timer_fd);
            }
            goto retry;
        }
    }
    return 0;
}



struct _Hook_initer 
{
    _Hook_initer()
    {
        hook_init();
    }

    void hook_init()
    {
        static bool is_inited = false;
        if(is_inited)
        {
            return;
        }

        #define XX(name) original_ ## name = (name ## _fun)dlsym(RTLD_NEXT, #name);
            HOOK_FUN(XX)
        #undef XX
    }
};
static _Hook_initer s_hook_inite;

bool is_hook_enable()
{
    return t_hook_enable;
}

void set_hook_state(bool flags)
{
    t_hook_enable = flags;
}



}






extern "C"
{
#define XX(name) name ## _fun original_ ## name = nullptr;
    HOOK_FUN(XX)
#undef XX

unsigned int sleep(unsigned int seconds)
{
    if(!m_sylar::is_hook_enable())
    {
        return original_sleep(seconds);
    }
    m_sylar::Fiber::ptr fiber = m_sylar::Fiber::GetThisFiber();
    m_sylar::IOManager* iom = m_sylar::IOManager::getInstance();
    m_sylar::TimeManager* TimeManager = m_sylar::TimeManager::getInstance();
    TimeManager->addTimer(seconds * 1000000, false, 
        [iom, fiber](){
        iom->schedule(fiber);
    });
    fiber->YieldToHold();
    return 0;
}

int usleep(useconds_t usec)
{
    if(!m_sylar::is_hook_enable())
    {
        return original_usleep(usec);
    }
    m_sylar::Fiber::ptr fiber = m_sylar::Fiber::GetThisFiber();
    m_sylar::IOManager* iom = m_sylar::IOManager::getInstance();
    m_sylar::TimeManager* TimeManager = m_sylar::TimeManager::getInstance();
    TimeManager->addTimer(usec, false, 
        [iom, fiber](){
        iom->schedule(fiber);
    });
    fiber->YieldToHold();
    return 0;
}

int nanosleep(const struct timespec *duration,
                struct timespec* rem)
{
    if(!m_sylar::is_hook_enable())
    {
        return original_nanosleep(duration, rem);
    }
    m_sylar::Fiber::ptr fiber = m_sylar::Fiber::GetThisFiber();
    m_sylar::IOManager* iom = m_sylar::IOManager::getInstance();
    m_sylar::TimeManager* TimeManager = m_sylar::TimeManager::getInstance();
    TimeManager->addTimer(duration->tv_sec * 1000000 + (duration->tv_nsec / 1000), false, 
        [iom, fiber](){
        iom->schedule(fiber);
    });
    fiber->YieldToHold();
    if(rem)
    {
    rem->tv_sec = 0;
    rem->tv_nsec = 0;
    }
    return 0; 
}

int socket(int domain, int type, int protocol)
{
    if(!m_sylar::is_hook_enable())
    {
        return original_socket(domain, type, protocol);
    }
    int fd = original_socket(domain, type, protocol);
    if(fd == -1)
    {
        return -1;
    }
    m_sylar::FdMgr::GetInstance()->get(fd, true);
    return fd;
}

int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen)
{
    int fd = do_io(sockfd, original_accept, "accept", m_sylar::IOManager::READ, SO_RCVTIMEO, addr, addrlen);
    if(fd == -1)
    {
        M_SYLAR_LOG_ERROR(m_sylar::g_logger) << "accept failed, sockfd=" << sockfd;
    }
    return fd;
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    return original_connect(sockfd, addr, addrlen);
}

// read
ssize_t read(int fd, void* buf, size_t count)
{
    return do_io(fd, original_read, "read", m_sylar::IOManager::READ, SO_RCVTIMEO,
                 buf, count);
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt)
{
    return do_io(fd, original_readv, "readv", m_sylar::IOManager::READ, SO_RCVTIMEO,
                 iov, iovcnt);
}

ssize_t preadv(int fd, const struct iovec *iov, int iovcnt, off_t offset)
{
    return do_io(fd, original_preadv, "preadv", m_sylar::IOManager::READ, SO_RCVTIMEO,
                 iov, iovcnt, offset);
}

ssize_t preadv2(int fd, const struct iovec *iov, int iovcnt, off_t offset, int flags)
{
    return do_io(fd, original_preadv2, "preadv2", m_sylar::IOManager::READ, SO_RCVTIMEO,
                 iov, iovcnt, offset, flags);
}

ssize_t recv(int sockfd, void* buf, size_t len, int flags)
{
    return do_io(sockfd, original_recv, "recv", m_sylar::IOManager::READ, SO_RCVTIMEO,
                buf, len, flags);
}

ssize_t recvfrom(int sockfd, void* buf, size_t len, int flags, struct sockaddr *  src_addr, socklen_t* addrlen)
{
    return do_io(sockfd, original_recvfrom, "recvfrom", m_sylar::IOManager::READ, SO_RCVTIMEO,
                buf, len, flags, src_addr, addrlen);
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags)
{
    return do_io(sockfd, original_recvmsg, "recvmsg", m_sylar::IOManager::READ, SO_RCVTIMEO,
                msg, flags);   
}


// write
ssize_t write(int fd, const void* buf, size_t count)
{
    return do_io(fd, original_write, "write", m_sylar::IOManager::WRITE, SO_SNDTIMEO,
                buf, count); 
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt)
{
    return do_io(fd, original_writev, "writev", m_sylar::IOManager::WRITE, SO_SNDTIMEO,
                iov, iovcnt); 
}
    
ssize_t pwritev(int fd, const struct iovec *iov, int iovcnt, off_t offset)
{
    return do_io(fd, original_pwritev, "pwritev", m_sylar::IOManager::WRITE, SO_SNDTIMEO,
                iov, iovcnt, offset); 
}

ssize_t pwritev2(int fd, const struct iovec *iov, int iovcnt, off_t offset, int flags)
{
    return do_io(fd, original_pwritev2, "pwritev2", m_sylar::IOManager::WRITE, SO_SNDTIMEO,
                iov, iovcnt, offset, flags); 
}

ssize_t send(int sockfd, const void* buf, size_t len, int flags)
{
    return do_io(sockfd, original_send, "send", m_sylar::IOManager::WRITE, SO_SNDTIMEO,
                buf, len, flags); 
}
    
ssize_t sendto(int sockfd, const void* buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen)
{
    return do_io(sockfd, original_sendto, "sendto", m_sylar::IOManager::WRITE, SO_SNDTIMEO,
                buf, len, flags, dest_addr, addrlen); 
}

ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags)
{
    return do_io(sockfd, original_sendmsg, "sendmsg", m_sylar::IOManager::WRITE, SO_SNDTIMEO,
                msg, flags); 
}

// close
int close(int fd){
    if(!m_sylar::is_hook_enable())
    {
        return original_close(fd);
    }

    m_sylar::FdCtx::ptr fd_ctx = m_sylar::FdMgr::GetInstance()->get(fd);

    if(fd_ctx)
    {   
        auto iom = m_sylar::IOManager::getInstance();
        if(iom)
        {
            iom->cancelAll(fd);
        }
        m_sylar::FdMgr::GetInstance()->del(fd);
    }
    return original_close(fd);
}

// functional       
int fcntl (int fd, int op, ...  )
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
                return original_fcntl(fd, op, arg);
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
            return original_fcntl(fd, op, arg);
        }
            break;
        case F_GETFL:
            {
                va_end(ap);
                int original_value = original_fcntl(fd, op);
                if(!m_sylar::is_hook_enable())
                {
                    return original_value;
                }

                m_sylar::FdCtx::ptr fd_ctx = m_sylar::FdMgr::GetInstance()->get(fd, false);
                if(!fd_ctx || fd_ctx->is_closed() || !fd_ctx->is_socket())
                {
                    // 非socket
                    return original_value;
                } 
                if(fd_ctx->getUserNoblock())
                {
                    return original_value | O_NONBLOCK;     // 用户设置非阻塞
                }
                else
                {
                    // return original_value & ~O_NONBLOCK;    // 用户没有规定非阻塞，默认阻塞
                    return original_value & ~O_NONBLOCK ;    // 用户没有规定非阻塞，默认阻塞，是实际非阻塞
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
            return original_fcntl(fd, op, arg);
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
            return original_fcntl(fd, op, arg);
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
            return original_fcntl(fd, op, arg);
        }
            break;

        // struct f_owner_ex*
        case F_GETOWN_EX:
        case F_SETOWN_EX:
        {
            struct f_owner_ex* arg = va_arg(ap, struct f_owner_ex*);
            va_end(ap);
            return original_fcntl(fd, op, arg);
        }
            break;
        
        default:
            va_end(ap);
            return original_fcntl(fd, op);
    }
}

int ioctl(int fd, unsigned long op, ...)
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
            return original_fcntl(fd, op, arg);
        }
        fd_ctx->setUserNoblock(user_nonblock);
    } 
    return original_ioctl(fd, op, 1);
}


// option
int getsockopt(int sockfd, int level, int optname,
                    void* optval,
                    socklen_t * optlen)
{
    return original_getsockopt(sockfd, level, optname, optval, optlen);
}

int setsockopt(int sockfd, int level, int optname,
                    const void* optval,
                    socklen_t optlen)
{
    if(!m_sylar::is_hook_enable())
    {
        return original_setsockopt(sockfd, level, optname, optval, optlen);
    }
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
    return original_setsockopt(sockfd, level, optname, optval, optlen);
}
}

