#include "../sylar/config.h"
#include "../sylar/log.h"
#include "../sylar/scheduler.h"
#include <string>

m_sylar::Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

int main(void) {
    std::cout << "test scheduler" << m_sylar::getThreadId() << std::endl;
    std::string name = "first";
    m_sylar::Scheduler sc(name, 1, true);
    sc.start();
    sc.stop();
    sleep(5);
    M_SYLAR_LOG_INFO(g_logger) << "main end";
    return 0;
}
