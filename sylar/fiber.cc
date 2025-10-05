#include "fiber.h"

namespace m_sylar {
static std::atomic<uint64_t> s_fiber_id {0};
static std::atomic<uint64_t> s_fiber_count {0};

static thread_local Fiber* t_fiber = nullptr;
static thread_local std::shared_ptr<Fiber::ptr> t_threadFiber = nullptr;

m_sylar::ConfigVar<uint32_t>::ptr g_fiber_stack_size =
    m_sylar::configManager::Lookup("fiber.stackSize", 512 * 1024, "fiber default stack size");

class MallocStackAllocator {
public: 
    static void* Alloc(size_t size) {
        return malloc(size);
    }
    static void* Dealloc(void* vp, size_t size){
        return free(vp);
    }
}

using StackAllocator = MallocStackAllocator;

Fiber::Fiber() {
    m_state = EXEC;
    Fiber::SetFiber(this);
    if(getcontext(&m_context)){
        M_SYLAR_ASSERT(false, "getcontext Failed");
    }
    ++s_fiber_count;

}

// 特殊函数
Fiber(std::function<void()> cb, size_t stackSize = 0)
    : m_id(++s_fiber_id), m_cb(cb){
    ++s_fiber_count;
    m_stackSize = (stackSize == 0) ? g_fiber_stack_size, stackSize;

    m_stack = StackAllocator::Alloc(m_stackSize);       // 生成栈
    if(!m_stack) {
        M_SYLAR_ASSERT(false, "Stack allocation failed");
    }
    if(getcontext(&m_context)) {
        M_SYLAR_ASSERT(false, "getcontext Failed");
    }
    m_context.uc_uclink = nullptr;
    m_context.uc_stack.ss_sp = m_stack;
    m_context.uc_stack.ss_size = m_stackSize;
    
    makecontext(&m_context, &Fiber::MainFunc, 0);

}
Fiber::~Fiber(){
    --s_fiber_count;
    if(m_stack ) {
        M_SYLAR_ASSERT(m_state == TERM || m_state == INIT);

        StackAllocator::Dealloc(m_stack, m_stackSize);
    }
    else {
        M_SYLAR_ASSERT(!m_cb);
        M_SYLAR_ASSERT(m_state == EXEC);
        Fiber* cur = t_fiber;
        if(cur == this) {
            SetFiber(nullptr);
        }
    }
}
void resetFunc(std::function<void()> cb);       // 重新分配任务函数与状态 (INIT, TERM)
// 协程切换
void swapIn();              // 切入
void swapOut();             // 切出

static Fiber::SetFiber(Fiber* fiber);    // 设置当前线程的协程 (t_threadFiber)
static Fiber::ptr GetThis();        // 获取当前协程
static void YieldToReady();         // 协程切换为后台并切换为状态Ready
static void YieldToHold();          // ^^^^^^^^^^^^^^^^^^^^^^^^Hold
static unsigned int TotalFiberNum();        // 获取总协程数

static MainFunc();


}