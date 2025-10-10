#pragma once
#include "config.h"
#include "fiber.h"
#include "log.h"
#include "thread.h"
#include <list>
#include <mutex>

namespace m_sylar {

static m_sylar::Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");


class Schedular {

// 支持协程的线程间切换
public:
    using ptr = std::shared_ptr<Schedular>;

    Schedular (size_t thread_num = 1, bool use_caller = true, const std::string& name);
    virtual ~Schedular ();

    std::string getName () {return m_name;}

    static Schedular* GetThis();        // 获取当前调度器
    static Fiber* GetMainFiber();       // 获取主协程
    // 线程池开始/停止
    void start ();
    void stop ();

    template <typename Fiber_or_Func>
    void schedular (Fiber_or_Func f, int threadId = -1) {
        bool need_tickle = false;
        {
            std::unique_lock<std::mutex> m_mutex;
            need_tickle = schedularNoLock(f, threadId);
        }
        if (need_tickle) {
            tickle();   
        }
    }

    template <typename InputOperator>
    void schedular(InputOperator begin, InputOperator end){
        bool need_tickle = false;
        {
            std::unique_lock<std::mutex> m_mutex;
            while (begin != end) {
                need_tickle = schedularNoLock((&*begin), threadId) || need_tickle;       // 第一次学到，新写法，get了
                begin++;
            }
        }
        if (need_tickle) {
            tickle();   
        }
    }

private:
    template <typename Fiber_or_Func>
    bool schedularNoLock (Fiber_or_Func f, int threadId) {
        bool need_tickle = m_fibers.empty();
        FiberAndThread ft (f, threadId);
        if(ft.fiber || ft.func){
            m_fibers.push_back(ft);
        }
        return need_tickle;
    }

protect:
    virtual void tickle();

private: 
    // 线程与任务   
    struct FiberAndThread {
        Fiber::ptr fiber;           // 任务协程
        std::function<void()> func;     // 任务函数 
        int thread;             // 运行线程Id

        FiberAndThread (Fiber::ptr f_ptr, int threadId) 
            : fiber(f_ptr), threadId(thread) {
            
        }
            
        // 传入智能指针的地址，约定swap使其引用计数减一。
        FiberAndThread (Fiber::ptr* f_ptr, int threadId) 
            : threadId(thread) {
                fiber.swap(*f_ptr);
        }

        FiberAndThread (std::function<void()> f, int threadId) 
            : func(f), threadId(thread) {
            
        }

        // 传入智能指针的地址，约定swap使其引用计数减一。
        FiberAndThread (std::function<void()>* f, int threadId) 
            : threadId(thread) {
                func.swap(*f);
        }

        FiberAndThread () : thread(-1) {};

        void reset () {
            fiber = nullptr;
            func = nullptr;
            thread = -1;
        }
    };

private:
    std::mutex m_mutex;                         // 资源互斥锁
    std::vector<Thread::ptr> m_threads;         // 管理的线程
    std::list<Fiber::ptr>;                      // 管理的协程
    std::string m_name;
};

}
