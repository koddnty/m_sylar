#include <iostream>
#include <memory>
#include "../sylar/self_timer.h"
#include "../sylar/hook.h"
#include "../sylar/allHeader.h"


static m_sylar::Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

int conunter = 0;

void ccb()
{
    M_SYLAR_LOG_DEBUG(g_logger) << "counter : " << conunter ++ << "是奇数";
}

bool condition()
{
    if(conunter % 2 == 0)
    {
        return false;
    }
    return true;
}

void condition_cb()
{
    M_SYLAR_LOG_DEBUG(g_logger) << "counter : " << conunter ++ << "是偶数";
}


void test1_aux1(m_sylar::IOManager::ptr iom, m_sylar::TimeManager::ptr tim);
void test1()
{   
    m_sylar::IOManager::ptr iom (new m_sylar::IOManager("timer_test"));


    m_sylar::TimeManager::ptr tim (new m_sylar::TimeManager(iom));

    test1_aux1(iom, tim);

    sleep(1);
    iom->stop();
}

void func()
{
    std::cout << "回调执行" << std::endl;
}


void test1_aux1(m_sylar::IOManager::ptr iom, m_sylar::TimeManager::ptr tim)
{

    bool tan = true;

    iom->schedule([tim](){
        // tim->addTimer(1 * 1000000, true, tim, ccb);
        int newfd = m_sylar::TimeManager::getInstance()->addConditionTimer(1 * 10000000, false, ccb, 
            []() 
            {
                // if(!tan)
                // {
                //     M_SYLAR_LOG_DEBUG(g_logger) << "p_tan is nullptr";
                //     return false;
                // }

                return false;
            }
        , func);

        original_sleep(3);
        std::cout << "自主取消timer" << ::std::endl;
        m_sylar::TimeManager::getInstance()->cancelTimer(newfd);
        std::cout << "取消timer结束" << ::std::endl;

        original_sleep(5);
    });

    sleep(1000);
}



int main(void) 
{
    test1();
    return 0;
}
