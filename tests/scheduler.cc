#include "../sylar/config.h"
#include "../sylar/log.h"
#include "../sylar/scheduler.h"
#include <string>

m_sylar::Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

int main(void) {
    std::cout << "test scheduler" << m_sylar::getThreadId() << std::endl;
    std::string name = "first";
    m_sylar::Scheduler sc(name, 1, false);
    sc.start();
    sleep(5);
    sc.stop();
    sleep(5);
    return 0;
}
