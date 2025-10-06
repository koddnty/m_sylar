#pragma once

#include "thread.h"
#include "log.h"
#include "config.h"
#include "macro.h"
#include <memory>
#include <ucontext.h>
#include <functional>

static m_sylar::Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

namespace m_sylar {

class Fiber : public std::enable_shared_from_this<Fiber> {
public:
    using ptr = std::shared_ptr<Fiber>;
    // 协程状态
    enum State {
        INIT,
        HOLD,
        EXEC,
        TERM,
        READY,
        EXCEPT
    };  

    // 特殊函数
    Fiber();
    Fiber(std::function<void()> cb, size_t stackSize = 0);      // 子协程构造函数
    ~Fiber();
    void resetFunc(std::function<void()> cb);       // 重新分配任务函数与状态 (INIT, TERM)
    // 协程切换
    void swapIn();              // 切入
    void swapOut();             // 切出

public:
    static void SetThisFiber(Fiber* fiber);                     // 设置当前线程的协程 (t_threadFiber)

    static Fiber::ptr GetThisFiber();                // 获取当前协程
    static void YieldToReady();                 // 协程切换为后台并切换为状态Ready
    static void YieldToHold();                  // ^^^^^^^^^^^^^^^^^^^^^^^^Hold
    static unsigned int TotalFiberNum();        // 获取总协程数

    static void MainFunc();

private:
    uint64_t m_fiberId = 0;
    uint64_t m_stackSize = 0;       // 栈大小
    State m_state = INIT;           // 协程状态
    ucontext_t m_context;            // 上下文
    void* m_stack = nullptr; 

    std::function<void()> m_cb;     // 任务函数
};

}