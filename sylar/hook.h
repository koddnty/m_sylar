#pragma once
#include "fiber.h"
#include "ioManager.h"
#include "self_timer.h"
#include <dlfcn.h>

namespace m_sylar
{
    bool is_hook_enable();
    void set_hook_state(bool flags);

}


extern "C"
{
    typedef unsigned int (*sleep_fun)(unsigned int seconds);
    extern sleep_fun sleep_f;

    typedef unsigned int (*usleep_fun)(useconds_t usec);
    extern usleep_fun usleep_f;

}