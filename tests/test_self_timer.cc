#include <iostream>
#include <memory>
#include "../sylar/self_timer.h"
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


    iom->stop();
}


void test1_aux1(m_sylar::IOManager::ptr iom, m_sylar::TimeManager::ptr tim)
{

    bool tan = true;
    std::shared_ptr<bool> p_tan {std::make_shared<bool>(true)};

    // tim->addTimer(1 * 1000000, true, tim, ccb);
    int newfd = tim->addConditionTimer(1 * 1000000, false, tim, ccb, 
        [&tan]() 
        {
            // if(!tan)
            // {
            //     M_SYLAR_LOG_DEBUG(g_logger) << "p_tan is nullptr";
            //     return false;
            // }
            if(tan)
            {
                return true;
            }
            else {
                return false;
            }
        }
    , [tim, newfd]()
    {
        M_SYLAR_LOG_DEBUG(g_logger) << "条件不满足，结束任务";
        tim->cancelTimer(newfd);
    });

    sleep(5);

    tan = false;

    sleep(5);

}



int main(void) 
{
    test1();
    return 0;
}
