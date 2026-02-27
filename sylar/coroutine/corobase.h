#pragma once
#ifdef SYLAR_USE_CORO20
    #include "coroutine/coro20/task.hpp"
    #include "coroutine/coro20/fiber.h"
    #include "coroutine/coro20/scheduler.h"
    #include "coroutine/coro20/ioManager.h"
    // #include "coroutine/coro20/hook.h"
#else
    #include "coroutine/ucontext/fiber.h"
    #include "coroutine/ucontext/scheduler.h"
    #include "coroutine/ucontext/ioManager.h"
    #include "coroutine/ucontext/hook.h"
#endif