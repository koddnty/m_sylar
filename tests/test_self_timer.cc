#include <iostream>
#include <memory>
#include "../sylar/self_timer.h"
#include "../sylar/allHeader.h"


static m_sylar::Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

int conunter = 0;

void ccb()
{
    M_SYLAR_LOG_DEBUG(g_logger) << "counter : " << conunter ++ ;
}

void test1()
{   
    m_sylar::IOManager::ptr iom (new m_sylar::IOManager("timer_test"));


    m_sylar::TimeManager::ptr tim (new m_sylar::TimeManager(iom));

    tim->addTimer(5 * 1000000, true, tim, ccb);


    sleep(20);
    iom->stop();
}

int main(void) 
{
    test1();
    return 0;
}
