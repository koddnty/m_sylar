#include "../sylar/hook.h"
#include "../sylar/ioManager.h"
#include <iostream>

m_sylar::Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

void test1()
{
    M_SYLAR_LOG_DEBUG(g_logger) << "begin";
    m_sylar::IOManager::ptr iom (new m_sylar::IOManager("test_hook", 1));
    iom->schedule([](){
        sleep(2);
        M_SYLAR_LOG_DEBUG(g_logger) << "sleep 2 sec";
    });

    iom->schedule([](){
        sleep(3);
        M_SYLAR_LOG_DEBUG(g_logger) << "sleep 3 sec";
    });

    sleep(2000);
    M_SYLAR_LOG_DEBUG(g_logger) << "end";
}

int main(void)
{
    test1();
    return 0;
}