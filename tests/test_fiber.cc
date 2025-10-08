#include "../sylar/fiber.h"
#include "../sylar/log.h"
#include "../sylar/config.h"
#include "../sylar/allHeader.h"
#include <iostream>



void runner_fiber () {
    m_sylar::Logger::ptr self_logger = M_SYLAR_GET_LOGGER_ROOT();
    m_sylar::Logger::ptr system_logger = M_SYLAR_LOG_NAME("system");
    M_SYLAR_LOG_INFO(system_logger) << "runner_fiber begin";
    m_sylar::Fiber::YieldToHold();
    M_SYLAR_LOG_INFO(system_logger) << "runner_fiber end";
    // m_sylar::Fiber::YieldToHold();
}

int main(void) {
    std::cout << "hello ? " << std::endl;
    m_sylar::Logger::ptr self_logger = M_SYLAR_GET_LOGGER_ROOT();
    {
        m_sylar::Fiber::GetThisFiber();
        M_SYLAR_LOG_INFO(self_logger) << "main begin";
        m_sylar::Fiber::ptr newFiber (new m_sylar::Fiber(runner_fiber));
        newFiber->swapIn();
        M_SYLAR_LOG_INFO(self_logger) << "main swapIn";
        newFiber->swapIn();
        M_SYLAR_LOG_INFO(self_logger) << "main end";
        // delete newFiber.get();
    }


    // std::flush;
    std::cout << "real END" << std::endl;
    
    return 0;
}


