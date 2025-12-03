#include "scheduler.h"
#include "log.h"
#include <algorithm>
#include <mutex>

namespace m_sylar {

    static thread_local Scheduler* tl_scheduler = nullptr;       // 线程内 调度器指针
    static thread_local Fiber* tl_fiber = nullptr;               // 线程内 主协程指针

    static m_sylar::Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

    Scheduler::Scheduler (const std::string& name, size_t thread_num, bool use_caller)
    : m_name(name), m_idleThreadCount(thread_num){
        M_SYLAR_ASSERT2(thread_num > 0, "Thread pool must have at least one thread\n");
        if(use_caller){
            std::cout << "use caller" << std::endl;
            m_sylar::Fiber::GetThisFiber();     
            --thread_num;  
            M_SYLAR_ASSERT(GetThis() == nullptr);
            tl_scheduler = this;
            m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this)));
            m_sylar::Thread::setName(m_name); 
            tl_fiber = m_rootFiber.get();
            m_rootThreadId = m_sylar::getThreadId();
            m_threadIds.push_back(m_rootThreadId);
        }
        else{
            m_rootThreadId = -1;
        }
        m_threadCount = thread_num;
    }

    Scheduler::~Scheduler (){
        M_SYLAR_ASSERT(m_stopping);
        if(GetThis() == this){
            tl_scheduler = nullptr;
        }
    }   

    void Scheduler::setThis(){
        tl_scheduler = this;
    }
    // 获取当前调度器
    Scheduler* Scheduler::GetThis(){
        return tl_scheduler;
    }        
    // 获取主协程
    Fiber* Scheduler::GetMainFiber(){
        return tl_fiber;
    }    

    // 线程池开始，新创建线程运行scheduler::run
    void Scheduler::start() {
        // std::unique_lock();
        m_stopping = false;
        M_SYLAR_ASSERT(m_threads.empty());
        m_threads.resize(m_threadCount);
        for (size_t i = 0; i < m_threadCount; i++){
            m_threads[i].reset(new Thread(std::bind(&Scheduler::run, this), m_name + "_" + std::to_string(i)));
            m_threadIds.push_back(m_threads[i]->getId());
        }
        if(m_rootFiber) {
            // useCaller
            Fiber::GetThisFiber();
            m_rootFiber->swapIn();
        }
    }
    // 线程池停止
    void Scheduler::stop() {
        m_autoStop = true;
        if (m_rootFiber && m_threadCount == 0 
            && (m_rootFiber->getState() == Fiber::TERM 
            ||  m_rootFiber->getState() == Fiber::INIT )){
            M_SYLAR_LOG_INFO(g_logger) << this << "stopped"; 
            m_stopping = true;
            
            if (stopping()) {
                return;
            }
        }

        // bool return_on_this_fiber = false;
        // if(m_rootThreadId != -1){
        //     // 线程是useCaller线程
        //     // M_SYLAR_ASSERT(Thread::getThis() == this);
        //     // return_on_this_fiber = true;
        // // } 
        // else {
        //     // M_SYLAR_ASSERT(Thread::getThis() == this);
        // }

        m_stopping = true;
        while(m_threadCount > 0) {
            tickle();       // 唤醒睡眠线程
            sleep(1);
        }

        if(m_rootFiber) {
            tickle();
        }
        if(stopping()) {
            return;
        }
        return;
    }
    // 线程运行函数
    void Scheduler::run() {
        setThis();              // 设置线程调度器
        if(m_sylar::getThreadId() != m_rootThreadId) {
            tl_fiber = Fiber::GetThisFiber().get();          // 设置线程主协程
        }
        Fiber::ptr idle_fiber(new Fiber(std::bind(&Scheduler::idle, this)));

        Fiber::ptr cb_fiber;        // 函数任务协程
        FiberAndThread ft;          
        while(true) {

            ft.reset();
            bool tickle_me = false;
            {
                std::unique_lock<std::mutex> lock (m_mutex);
                auto it = m_tasks.begin();
                while (it != m_tasks.end()) {
                    // 任务获取
                    if (it->thread != -1 && it->thread != m_sylar::getThreadId()) {
                        // 任务线程被指定，通知其他线程
                        ++it;
                        tickle_me = true;
                        continue;
                    }
                    
                    M_SYLAR_ASSERT2(it->fiber || it->func, 
                                    "the task is NULL");
                    if(it->fiber && it->fiber->getState() == Fiber::EXEC) {
                        // 结束的任务，跳过
                        ++it;
                        continue;
                    }
                    ft = *it;
                    m_tasks.erase(it++);
                    break;
                }
            }

            if(tickle_me) {
                M_SYLAR_LOG_DEBUG(g_logger) << "need tickle othres";
                tickle();
            }
            
            // 停止检测/协程任务/函数任务/无任务
            if(m_autoStop)
            {
                // 线程池停止
                M_SYLAR_LOG_DEBUG(g_logger) << "child thread stop";
                break;
            }
            else if(ft.fiber && (ft.fiber->getState() != Fiber::TERM
                        || ft.fiber->getState() != Fiber::EXCEPT)) {
                // 协程任务
                ++m_activeThreadCount;
                // std::cout << "1" << std::endl;
                ft.fiber->swapIn();
                --m_activeThreadCount;
                
                // 结束单次运行后处理
                if(ft.fiber->getState() == Fiber::READY) {
                    // 运行未完成
                    schedule(ft.fiber);
                }
                else if (ft.fiber->getState() != Fiber::TERM 
                        && ft.fiber->getState() != Fiber::EXCEPT){
                    // 协程暂停
                    ft.fiber->m_state = Fiber::HOLD;
                }
                ft.reset();
            } 
            else if(ft.func) {
                // 函数任务
                cb_fiber.reset(new Fiber(ft.func));
                ft.reset();
                ++m_activeThreadCount;
                // std::cout << "2" << std::endl;
                cb_fiber->swapIn();
                --m_activeThreadCount;
                if(cb_fiber->getState() == Fiber::READY) {
                    while(cb_fiber->getState() == Fiber::READY){
                        schedule(cb_fiber);
                    }
                    cb_fiber.reset();
                }
                else if (cb_fiber->getState() == Fiber::EXCEPT 
                        || cb_fiber->getState() == Fiber::TERM) {
                    cb_fiber->resetFunc(nullptr);
                }
                else {
                    cb_fiber->m_state = Fiber::HOLD;
                    cb_fiber.reset();
                }
            }
            else {
                // 当前线程空闲， 执行idle
                if(idle_fiber->getState() == Fiber::TERM) {
                    break;
                }
                ++m_idleThreadCount;
                // std::cout << "3" << std::endl;
                idle_fiber->swapIn();
                --m_idleThreadCount;
                if (idle_fiber->getState() != Fiber::EXCEPT 
                    && idle_fiber->getState() != Fiber::TERM) {
                    // 没有结束
                    idle_fiber->m_state = Fiber::HOLD;
                }
            }
        }

        // 线程池终止 执行清理
        std::unique_lock<std::mutex> pool_lock(m_mutex);
        --tl_scheduler->m_threadCount;  // 减少线程池内部线程数量。
    }

    // 特定线程唤醒函数
    void Scheduler::tickle(){
        M_SYLAR_LOG_INFO(g_logger) << "tickle others";
    }
    bool Scheduler::stopping(){
        bool State;
        {
            std::unique_lock<std::mutex> lock (m_mutex);
            State = m_autoStop && m_stopping && m_tasks.empty() && m_activeThreadCount == 0;
        }
        // M_SYLAR_LOG_INFO(g_logger) << "thread pool stopping check : " << State;
        return State;
    }
    // 空转函数，线程空闲时运行
    void Scheduler::idle(){
        M_SYLAR_LOG_DEBUG(g_logger) << "thread " << m_sylar::getThreadId() << "is idle";
    }
}
