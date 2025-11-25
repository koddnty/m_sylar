#include "../sylar/fiber.h"
#include "../sylar/log.h"
#include "../sylar/config.h"
#include "../sylar/allHeader.h"
#include <iostream>
    
static m_sylar::Logger::ptr self_logger = M_SYLAR_GET_LOGGER_ROOT(); 

void runner_fiber () {
    m_sylar::Logger::ptr self_logger = M_SYLAR_GET_LOGGER_ROOT();
    m_sylar::Logger::ptr system_logger = M_SYLAR_LOG_NAME("system");
    M_SYLAR_LOG_INFO(system_logger) << "runner_fiber begin";
    m_sylar::Fiber::YieldToHold();
    M_SYLAR_LOG_INFO(system_logger) << "runner_fiber end";
    // m_sylar::Fiber::YieldToHold();
}

void runner_thread() {
    m_sylar::Fiber::GetThisFiber();
    M_SYLAR_LOG_INFO(self_logger) << "main begin";
    m_sylar::Fiber::ptr newFiber (new m_sylar::Fiber(runner_fiber));
    newFiber->swapIn();
    M_SYLAR_LOG_INFO(self_logger) << "main swapIn";
    newFiber->swapIn();
    M_SYLAR_LOG_INFO(self_logger) << "main end";
}


int main(void) {

    
    // YAML::Node loadYaml = YAML::LoadFile("/home/ls20241009/user/code/project/sylar_cp/m_sylar/conf/basic.yaml");
    // m_sylar::configManager::LoadFromYaml(loadYaml);

    // std::vector<m_sylar::Thread::ptr> vec;
    // for(int i = 0; i < 3; i++){
    //     m_sylar::Thread::ptr thr(new m_sylar::Thread(std::function<void()> (runner_thread), "func_" + std::to_string(i)));
    //     vec.push_back(thr);
    // }
    // for(auto& it : vec){
    //     it->join();
    // }
    // return 0;


    runner_thread();
    return 0;
}


