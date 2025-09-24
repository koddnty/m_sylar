#pragma once
#include <thread>
#include "until.h"
#include <pthread.h>
#include <functional>
#include <memory>
#include <string>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>


namespace m_sylar{

class Thread {
public:
    using ptr = std::shared_ptr<Thread>;

    Thread(std::function<void()> cb, const std::string& name);
    ~Thread();
    
    pid_t getId() const {return m_id;}              // 获取线程id   
    void join();                                    // 等待线程执行结束 
    static void* run (void* args);                  // 运行线程函数
    
    static Thread* getThis();                       // 获取当前线程的控制类
    static const std::string& getName();            // 获取当前线程名称
    static void setName(const std::string& name );        // 设置线程名称



private: 
    pid_t m_id;                                     // 线程id(程序内部)
    pthread_t m_thread;                             // POSIX id       0 空闲  <other> 繁忙 
    std::function<void()> m_cb;                     // 线程任务函数
    std::string m_name;                             // 线程自定义名称

private:            
    Thread(const Thread&) = delete;
    Thread(const Thread&&) = delete;
    Thread operator=(const Thread&) = delete;
}; 
   
}