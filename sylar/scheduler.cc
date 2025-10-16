#include "scheduler.h"

namespace m_sylar {

    static thread_local Scheduler* tl_scheduler = nullptr;       // 线程内 调度器指针
    static thread_local Fiber* tl_fiber = nullptr;               // 线程内 主协程指针

    Scheduler::Scheduler (size_t thread_num = 1, bool use_caller = true, const std::string& name)
    : m_idleThreadCount(thread_num), m_name(name) {
        M_SYLAR_ASSERT2(m_threadCount > 0, "Thread pool must have at least one thread\n");
        if(use_caller){
            m_sylar::Fiber::GetThisFiber();     
            --thread_num;  
            M_SYLAR_ASSERT(GetThis()) == nullptr;
            tl_scheduler = this;
            m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this)));
            m_sylar::Thread::setName(m_name); 
            tl_fiber = m_rootFiber.get();
            m_rootThreadId = m_sylar::Thread::getId();
            m_threadIds.push_back(m_rootThreadId);
        }
        else{
            m_rootThreadId = -1;
        }
        m_threadCount = thread_num; 
    }

    virtual Scheduler::~Scheduler (){
        M_SYLAR_ASSErt(m_stopping);
        if(GetThis() == this){
            tl_scheduler = nullptr;
        }
        
    }   
  
    std::string Scheduler::getName () {return m_name;}

    static Scheduler::Scheduler* GetThis();        // 获取当前调度器
    static Scheduler::Fiber* GetMainFiber();       // 获取主协程
    // 线程池开始/停止
    void Scheduler::start (){
        std::unique_lock()
        m_stopping = false;
        M_SYLAR_ASSERT(m_threadIds.empty());

        m_threadIds.resize(m_threadCount);
        for (size_t i = 0; i < m_threadCount; i++){
            m_threadIds[i].reset(new Thread(std::bind(&Scheduler::run. this), m_name + "_" + std::to_string(i)));
            m_threadIds.push_back(m_threadIds[i]->getId());
        }

    }
    void Scheduler::stop(){
        m_autoStop = true;
        if (m_rootFiber && m_threadCount == 0 
            && (m_rootFiber->getState() == Fiber::TERM 
            ||  m_rootFiber->getState() == Fiber::INIT )){
            M_SYLAR_LOG_INFO(m_logger) << this << "stopped"; 
            m_stoppig = true;
            
            if (stopping()) {
                return;
            }
        }

        bool return_on_this_fiber = false;
        if(m_rootThread != -1){
            // 线程是useCaller线程
            M_SYLAR_ASSERT(getThis() == this);
            return_on_this_fiber = true;
        } 
        else {
            M_SYLAR_ASSERT(getThis() == this);
        }

        m_stopping = true;
        for(size_t i = 0; i < m_threadCount; i++) {
            tickle();       // 唤醒睡眠线程
        }

        // if (return_on_this_fiber = true) {

        // }
        if(m_rootFiber) {
            tickle();
        }
        if(stopping()) {
            return;
        }
    }

        void Scheduler::run() {
            setThis();
            if(m_sylar::getThreadId != m_rootThread) {
                tl_fiber = Fiber::GetThis().get();          // 设置线程主协程
            }

            Fiber::ptr idle_fiber (new Fiber());
        }

}
