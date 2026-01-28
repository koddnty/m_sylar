#include "basic/config.h"
#include "basic/log.h"
// #include "basic/scheduler.h"
#include "coroutine/corobase.h"
#include <string>

m_sylar::Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

void func() {
    M_SYLAR_LOG_DEBUG(g_logger) << "func is welld runed";
}

int main(void) {
    std::cout << "test scheduler" << m_sylar::getThreadId() << std::endl;
    std::string name = "first";
    m_sylar::Scheduler sc(name, 20, false);
    for (int i = 0; i < 20; i++)
    {
        sc.schedule(func);
    }
    sc.start();
    sleep(3);

    sc.stop();
    sleep(5);
    M_SYLAR_LOG_INFO(g_logger) << "main end";
    return 0;
}
