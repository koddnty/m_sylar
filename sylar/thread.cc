#include "thread.h"
#include "log.h"

namespace m_sylar{

static thread_local Thread* t_thread = nullptr;                         // 线程管理类地址
static thread_local std::string t_thread_name = "UNKNOWN";              // 线程名称

static m_sylar::Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

Thread* Thread::getThis(){
    return t_thread;
}


const std::string& Thread::getName(){
    return t_thread_name;
}
void Thread::setName(std::string& name){
    if(t_thread){
        t_thread->setName(name);
    }
    t_thread_name = name;
}


Thread::Thread(std::function<void()> cb, const std::string& name = "UNKNOWN"){
    m_cb = cb;
    m_name = name;
    t_thread_name = name;
    int rt = pthread_create(&m_thread, nullptr, &Thread::run, this);
    if(rt){
        M_SYLAR_LOG_ERROR(g_logger) << "pthread_create failed"
            << "thread_name = " << name;
        throw std::logic_error("pthread_create error");
    }
}


Thread::~Thread () {
    pthread_detach(m_thread);  
}


void Thread::join(){
    if(m_thread){
        int rt = pthread_join(m_thread, nullptr);
        if(rt){
            M_SYLAR_LOG_ERROR(g_logger) << "pthread_join thread failed rt = " << rt 
             << " m_name = " << m_name;
            throw std::logic_error("pthread_join thread failed");
        }
    }
    m_thread = 0;
}


void* Thread::run(void* args){
    Thread* thread = (Thread*) args;
    t_thread = thread;
    thread->m_id = m_sylar::getThreadId();
    pthread_setname_np (pthread_self(), thread->m_name.substr(0, 15).c_str());          // 为线程命名
    std::function<void()> cb;
    cb.swap(thread->m_cb);
    cb();
    return 0;
}

// pid_t Thread::getId(){
// }



}