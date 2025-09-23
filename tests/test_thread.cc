#include "../sylar/allHeader.h"
#include <iostream>
#include <vector>



m_sylar::Logger::ptr g_logger = M_SYLAR_GET_LOGGER_ROOT();

void func1(){
    
    M_SYLAR_LOG_INFO(g_logger) << " func1 开始"
         << "  name =  "  << m_sylar::Thread::getName()
         << " id: " << m_sylar::getThreadId()
         << " this.id: " << m_sylar::Thread::getThis()->getId();

    sleep(20);
}

void func2(){

}


int main(void){
    std::vector<m_sylar::Thread::ptr> vec;
    for(int i = 0; i < 5; i++){
        m_sylar::Thread::ptr thr(new m_sylar::Thread(std::function<void()> (func1), "name_" + std::to_string(i)));
        vec.push_back(thr);
    }
    for(int i = 0; i < 5; i++){
        vec[i]->join();
    }
    return 0;
}
