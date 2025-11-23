#include "../sylar/log.h"
#include <iostream>

m_sylar::Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

int main(void) {
    {
        M_SYLAR_LOG_DEGUB(g_logger) << "testa";
    }
    
    return 0;
}
