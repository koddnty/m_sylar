#include "hook.h"



namespace m_sylar
{

static thread_local bool t_hook_enable = false;

#define HOOK_FUN(XX) \
    XX(sleep) \
    XX(usleep)


void hook_init()
{
    static bool is_inited = false;
    if(is_inited)
    {
        return;
    }
#define XX(name) name ## _f = (name ## _fun)dlsym(RTLD_NEXT, #name);
    HOOK_FUN(XX)
#undef XX
}


struct _Hook_initer 
{
    _Hook_initer()
    {
        hook_init();
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
#define XX(name) name ## _fun name ## _f = nullptr;
    HOOK_FUN(XX)
#undef XX



unsigned int sleep(unsigned int seconds)
{
    if(!m_sylar::is_hook_enable())
    {
        return sleep_f(seconds);
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
        return sleep_f(usec);
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
}


// extern sleep_fun sleep_f;


// extern usleep_fun usleep_f;