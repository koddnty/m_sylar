#include "schedular.h"

namespace m_sylar {


    Schedular (size_t thread_num = 1, bool use_caller = true, const std::string& name) {

    }

    virtual ~Schedular ();

    std::string getName () {return m_name;}

    static Schedular* GetThis();        // 获取当前调度器
    static Fiber* GetMainFiber();       // 获取主协程
    // 线程池开始/停止
    void start ();
    void stop ();
}