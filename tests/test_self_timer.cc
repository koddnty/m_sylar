#include <iostream>
#include <memory>
#include "basic/self_timer.h"
#include "coroutine/corobase.h"
#include "coroutine/corobase.h"
#include <catch2/catch_test_macros.hpp>
// #include "basic/hook.h"catch_test_macros

using namespace m_sylar;    

bool test_condition(int need){
    bool TimerFinished = false;
    bool LoopFinished = false;

    std::atomic<int> count = 0;

    IOManager iom("test_timer", 1);
    TimeManager tim(&iom);
    int newfd = -1;
    newfd = tim.addConditionTimer(1 * 100000, true, 
        [&TimerFinished, &LoopFinished]() ->Task<bool> {
            TimerFinished = true;
            LoopFinished = true;
            co_return false;
        }, 
        [&count, &TimerFinished, &LoopFinished, need]() ->Task<bool> {
            if(count < need) {
                count++;
                co_return false;
            }
            if(TimerFinished == true) {
                // 循环结束还在跑，说明有问题
                std::cout << "TimerFinished is true but loop is still running, count = " << count << std::endl;
                LoopFinished = false;
                co_return false;
            }
            co_return true;
        }, 
        []()->Task<bool>{
            co_return true;
        });

    sleep(need / 10 + 1);

    iom.stop();
    if(count >= need && TimerFinished && LoopFinished) {
        return true;
    }
    return false;
}



// void test2() {


//     std::shared_ptr<m_sylar::TimeLimitInfo::State> eventState = std::make_shared<m_sylar::TimeLimitInfo::State>();
//     m_sylar::TimeManager::getInstance()->addEventWithTimeout(10, m_sylar::FdContext::READ, timeoutTask, 4000000, eventState);

//     sleep(10);
//     std::cout << "task State: " << *eventState;
//     return;
// }


TEST_CASE("test self timer", "[self_timer]")
{
    for(int i = 2; i < 20; i++) {
        REQUIRE(true == test_condition(i));
    }
}
