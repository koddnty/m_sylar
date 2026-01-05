#include "basic/allHeader.h"
#include "basic/macro.h"

m_sylar::Logger::ptr g_logger = M_SYLAR_GET_LOGGER_ROOT();

void func1() {
    M_SYLAR_LOG_INFO(g_logger) << std::endl << m_sylar::BackTraceToStr(7);
}

void func2(){
    // M_SYLAR_ASSERT (1);
    // std::cout << "-----------------------------" << std::endl;
    // M_SYLAR_ASSERT (0);
    // std::cout << "=============================" << std::endl;
    M_SYLAR_ASSERT2 (1, "嘻嘻哈哈");
    std::cout << "-----------------------------" << std::endl;
    M_SYLAR_ASSERT2 (0, "嘻嘻哈哈");
}

int main(void ){
    func2();
    return 0;
}