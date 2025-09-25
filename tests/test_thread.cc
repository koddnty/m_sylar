#include "../sylar/allHeader.h"
#include <iostream>
#include <vector>




// void func1(){
//         M_SYLAR_LOG_INFO(g_logger) << " func1 开始"
//          << "  name =  "  << m_sylar::Thread::getName()
//          << " id: " << m_sylar::getThreadId()
//          << " this.id: " << m_sylar::Thread::getThis()->getId();

//     // sleep(20);
// }

void func2(){
    m_sylar::Logger::ptr sys_logger = M_SYLAR_LOG_NAME("system2");
    for(int i = 0; i < 1; i++){
        M_SYLAR_LOG_INFO(sys_logger) << "222222222222222222222222222222222222222";
    }
}   

void func3(){
    m_sylar::Logger::ptr sys_logger = M_SYLAR_LOG_NAME("system2");
    for(int i = 0; i < 1; i++){
        M_SYLAR_LOG_INFO(sys_logger) << "333333333333333333333333333333333333333";
    }
}   

int main(void){
    YAML::Node loadYaml = YAML::LoadFile("/home/ls20241009/user/code/project/sylar_cp/m_sylar/conf/basic.yaml");
    m_sylar::configManager::LoadFromYaml(loadYaml);

    std::vector<m_sylar::Thread::ptr> vec;
    for(int i = 0; i < 5; i++){
        m_sylar::Thread::ptr thr2(new m_sylar::Thread(std::function<void()> (func2), "func2_" + std::to_string(i)));
        m_sylar::Thread::ptr thr3(new m_sylar::Thread(std::function<void()> (func3), "func3_" + std::to_string(i)));
        vec.push_back(thr2);
        vec.push_back(thr3);
    }
    for(auto& it : vec){
        it->join();
    }
    return 0;
}
