#include "../sylar/hook.h"
#include "../sylar/ioManager.h"
#include <ctime>
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
        usleep(3000000);
        M_SYLAR_LOG_DEBUG(g_logger) << "sleep 3 sec";
    });

    iom->schedule([](){
        timespec timer;
        timer.tv_nsec = 10000000000;
        timer.tv_sec = 5;
        nanosleep(&timer, NULL);
        M_SYLAR_LOG_DEBUG(g_logger) << "sleep 5s-0n";
    });

    sleep(2000);
    M_SYLAR_LOG_DEBUG(g_logger) << "end";
}

int main(void)
{
    test1();
    return 0;
}