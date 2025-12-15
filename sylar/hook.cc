#include "hook.h"
#include <asm-generic/errno.h>
#include <cerrno>
#include <cstdint>
#include <memory>
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
    uint32_t event, int type, ssize_t buflen, Args&& ... args)
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

}

