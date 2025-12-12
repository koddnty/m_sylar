#include "hook.h"


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
    m_sylar::IOManager* iom = m_sylar::IOManager::getThis();
    m_sylar::TimeManager* TimeManager = m_sylar::TimeManager::getThis();
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
    m_sylar::IOManager* iom = m_sylar::IOManager::getThis();
    m_sylar::TimeManager* TimeManager = m_sylar::TimeManager::getThis();
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
    m_sylar::IOManager* iom = m_sylar::IOManager::getThis();
    m_sylar::TimeManager* TimeManager = m_sylar::TimeManager::getThis();
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

