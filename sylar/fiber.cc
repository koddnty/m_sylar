#include "fiber.h"

namespace m_sylar {
static m_sylar::Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

static std::atomic<uint64_t> s_fiber_id {0};
static std::atomic<uint64_t> s_fiber_count {0};

static thread_local Fiber* t_fiber = nullptr;                   // 线程内 正在执行的子fiber指针
static thread_local Fiber::ptr t_threadFiber = nullptr;         // 线程内 主fiber智能指针
m_sylar::ConfigVar<uint64_t>::ptr g_fiber_stack_size =
    m_sylar::configManager::Lookup("fiber.stacksize", (uint64_t)(512 * 1024), "fiber default stack size");

class MallocStackAllocator {
public: 
    static void* Alloc(size_t size) {
        return malloc(size);
    }
    static void Dealloc(void* vp, size_t size){
        free((ucontext_t*)vp);
        return ;
    }
};

using StackAllocator = MallocStackAllocator;

Fiber::Fiber() {
    m_state = EXEC;
    SetThisFiber(this);     
    if(getcontext(&m_context)){
        M_SYLAR_ASSERT2(false, "getcontext Failed");
    }
    ++s_fiber_count;
}

// 特殊函数
Fiber::Fiber(std::function<void()> cb, uint64_t stackSize)
    : m_fiberId(++s_fiber_id), m_cb(cb){
    ++s_fiber_count;
    m_stackSize = (stackSize == 0) ? g_fiber_stack_size->getValue() : stackSize;

    m_stack = StackAllocator::Alloc(m_stackSize);       // 生成栈
    if(!m_stack) {
        M_SYLAR_ASSERT2(false, "Stack allocation failed");
    }
    if(getcontext(&m_context)) {
        M_SYLAR_ASSERT2(false, "getcontext Failed");
    }
    m_context.uc_link = nullptr;
    m_context.uc_stack.ss_sp = m_stack;
    m_context.uc_stack.ss_size = m_stackSize;
    
    makecontext(&m_context, &Fiber::MainFunc, 0);

}
Fiber::~Fiber(){
    --s_fiber_count;
    std::cout << "~Fiber : " << m_fiberId << "threadId" << m_sylar::getThreadId() << std::endl; 
    // M_SYLAR_LOG_INFO(g_logger) << "~Fiber : " << m_fiberId << "threadId" << m_sylar::getThreadId();
    if(m_stack) {
        // 协程有栈子协程
        M_SYLAR_ASSERT(m_state == TERM || m_state == INIT);

        StackAllocator::Dealloc(m_stack, m_stackSize);
    }
    else {
        // 主协程
        M_SYLAR_ASSERT(!m_cb);
        M_SYLAR_ASSERT(m_state == EXEC);
        Fiber* cur = t_fiber;               // 确认当前协程是否为主协程
        if(cur == this) {
            // 确认后设置当前线程内运行协程为空。协程关闭
            SetThisFiber(nullptr);
        }
    }
}

// 重新分配任务函数与状态 (对已分配栈的重新利用) (INIT, TERM)
void Fiber::resetFunc(std::function<void()> cb){        
    M_SYLAR_ASSERT(m_stack);
    M_SYLAR_ASSERT(m_state == TERM ||
                   m_state == INIT);
    m_cb = cb;
    if(getcontext(&m_context)){
        M_SYLAR_ASSERT2(false, "getContext");
    }

    m_context.uc_link = nullptr;
    m_context.uc_stack.ss_sp = m_stack;
    m_context.uc_stack.ss_size = m_stackSize;

    makecontext(&m_context, &Fiber::MainFunc, 0);
    m_state = INIT;
}       
// 协程切换
void Fiber::swapIn(){              // 主协程->当前协程
    SetThisFiber(this);
    M_SYLAR_ASSERT(m_fiberId && m_state != EXEC);
    m_state = EXEC;
    if (swapcontext(&(t_threadFiber->m_context), &m_context)) {
        M_SYLAR_ASSERT2(false, "swapContext");
    }
}
void Fiber::swapOut(){             // 当前协程->主协程
    SetThisFiber(t_threadFiber.get());
    if (swapcontext(&(m_context), &(t_threadFiber->m_context))) {
        M_SYLAR_ASSERT2(false, "swapContext");
    }
}

void Fiber::SetThisFiber(Fiber* fiber){          // 设置当前线程的协程 (t_threadFiber)
    t_fiber = fiber;
}
Fiber::ptr Fiber::GetThisFiber(){                  // 获取当前协程 (若线程无主协程将自动创建主协程)
    if(t_fiber != nullptr) {
        return t_fiber->shared_from_this();
    }
    // M_SYLAR_LOG_INFO(g_logger) << "just test g_logger";
    Fiber::ptr main_fiber(new Fiber);
    M_SYLAR_ASSERT(t_fiber == main_fiber.get());
    t_threadFiber = main_fiber;
    return t_fiber->shared_from_this();
}
void Fiber::YieldToReady(){                     // 协程切换为后台并切换为状态Ready
    Fiber::ptr cur_fiber = GetThisFiber();
    cur_fiber->m_state = READY;
    M_SYLAR_ASSERT2(cur_fiber->getFiberId() != 0, "fiber 0 can not be yield to ready");
    cur_fiber->swapOut();
}                 
void Fiber::YieldToHold(){                      // ^^^^^^^^^^^^^^^^^^^^^^^^Hold
    Fiber::ptr cur_fiber = GetThisFiber();
    cur_fiber->m_state = HOLD;
    M_SYLAR_ASSERT2(cur_fiber->getFiberId() != 0, "fiber 0 can not be yield to hold");
    cur_fiber->swapOut();
}
unsigned int Fiber::TotalFiberNum(){ // 获取总协程数
    return s_fiber_count;
}   

// 协程 执行函数
void Fiber::MainFunc() {
    Fiber::ptr cur_fiber = GetThisFiber();
    M_SYLAR_ASSERT(cur_fiber);
    try {
        cur_fiber->m_cb();
        cur_fiber->m_cb = nullptr;
        cur_fiber->m_state = TERM;
    } catch (std::exception& excp) {
        cur_fiber->m_state = EXCEPT;
        M_SYLAR_LOG_ERROR(g_logger) << "Fiber except : " << excp.what();
    } catch (...) {
        cur_fiber->m_state = EXCEPT;
        M_SYLAR_LOG_ERROR(g_logger) << "Fiber except";
    }

    auto raw_ptr = cur_fiber.get();
    cur_fiber.reset();
    raw_ptr->swapOut();

    M_SYLAR_ASSERT2(false, "fiber has already been destoried, but still swaped in.");
}



}
