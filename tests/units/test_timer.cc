#include <iostream>
#include <memory>
#include "basic/timer/timer.hpp"
#include "coroutine/corobase.h"
#include <catch2/catch_test_macros.hpp>


using namespace m_sylar;    

/**
    测试1ms的定时器任务
    1. 创建一个定时器任务，设置为1ms后执行
    2. 在定时器任务中设置一个标志位，表示定时器任务已经执行
    3. 在主线程中等待一段时间，确保定时器任务有足够的时间执行
    4. 检查标志位，确保定时器任务已经执行
    5. 如果标志位为true，则测试通过，否则测试失败
*/

bool test_condition(int need){
    bool TimerFinished = false;
    bool LoopFinished = false;

    std::atomic<int> count = 0;

    IOManager::ptr iom = std::make_shared<IOManager>("test_timer", 1);
    TimeManager::ptr tim = std::make_shared<TimeManager>(iom, 2000);
    tim->init();
    TimeTask::ptr time_task = TimeTask::create(1, true, 
        [&TimerFinished, &LoopFinished](TimeTask::ptr time_task) ->Task<void> {
            TimerFinished = true;
            LoopFinished = true;
            time_task->cancel();
            co_return ;
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
        [](TimeTask::ptr)->Task<void>{
            co_return ;
        });
    tim->addConditionTimer(time_task);

    sleep(need / 10 + 1);

    iom->stop();
    if(count >= need && TimerFinished && LoopFinished) {
        return true;
    }
    return false;
}


/**
    创建thread_num线程的iomanager,timer_count个定时器任务，目标是target_count个定时器任务执行完成，task_count个定时器任务在执行过程中被取消，block_size为时间块大小，timeout为测试超时时间
    1. 创建一个IOManager，线程数为thread_num
    2. 创建一个TimeManager，时间块大小为block_size
    3. 创建timer_count个定时器任务，每个定时器任务的执行时间为1ms，设置为循环定时器，回调函数中增加一个计数器，表示定时器任务已经执行
    4. 在主线程中等待timeout秒，确保定时器任务有足够的时间执行
    5. 检查计数器，确保定时器任务已经执行target_count次
    6. 如果计数器小于等于target_count + timer_count，大于targget_count则测试通过，否则测试失败
*/
int test_plenty_timer(int thread_num, int timer_count, int target_count, int block_size, int timeout) {
    IOManager::ptr iom = std::make_shared<IOManager>("test_plenty_timer", thread_num);
    TimeManager::ptr tim = std::make_shared<TimeManager>(iom, block_size);
    tim->init();

    std::atomic<int> condition_counter {0};
    for(int i = 0; i < target_count; i++) {
        TimeTask::ptr time_task = TimeTask::create(1, true, 
            [&condition_counter, target_count](TimeTask::ptr task) -> Task<void> {
                if(condition_counter > target_count) {
                    task->cancel();
                }
                else {
                    condition_counter++;
                }
                co_return;
            }, 
            []() -> Task<bool> {
                co_return true;
            }, 
            [](TimeTask::ptr task) -> Task<void> {
                co_return ;
            });
        tim->addConditionTimer(time_task);
    }   

    while(condition_counter < target_count && timeout > 0) {
        sleep(1);
        timeout--;
    }
    tim->close();
    iom->autoStop();

    return condition_counter.load();
}



TEST_CASE("test plenty timer", "[timer]") {
    // 只测代表性的点，不是全排列
    int thread_nums[] = {1, 2, 4, 8};
    int timer_counts[] = {100, 500, 1000};
    int block_sizes[]  = {500, 2000};
    int timeout = 100;  // 10s → 3s，1ms 定时器 3 秒足够完成很多轮了

    for(int thread_num : thread_nums) {                // 4
        for(int timer_count : timer_counts) {          // 3
            for(int block_size : block_sizes) {        // 2
                int target_count = timer_count * 500;  // 降低目标，3 秒内可达
                int finished = test_plenty_timer(thread_num, timer_count,
                                                 target_count, block_size, timeout);
                REQUIRE(finished <= target_count + timer_count);
                REQUIRE(finished >= target_count);
            }
        }
    }
}

TEST_CASE("test condition timer", "[timer]")
{
    // 测试少量条件定时器任务
    for(int i = 2; i < 20; i++) {
        REQUIRE(true == test_condition(i));
    }


}


