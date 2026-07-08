#include "basic/timer/timer.hpp"
#include <sys/timerfd.h>


using namespace m_sylar;

static m_sylar::Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

int test_base_timer() {
    IOManager::ptr iom = std::make_shared<IOManager>("test_timer", 1);
    TimeManager::ptr tim = std::make_shared<TimeManager>(iom, 2000);
    auto begin = TimeManager::GetCurrentMS();
    M_SYLAR_LOG_INFO(g_logger) << "start test timer, time= " << begin;

    tim->init();
    for(int i = 0; i < 1; i++) {
        TimeTask::ptr time_task = TimeTask::create(1 + 50 * i, true, [](TimeTask::ptr task) -> Task<void> {
            M_SYLAR_LOG_INFO(g_logger) << "time task executed";
            co_return ;
        });
        tim->addTimer(time_task);
    }   

    sleep(10);
    M_SYLAR_LOG_INFO(g_logger) << "test timer finished, time(from" << begin << ")= " << TimeManager::GetCurrentMS();

    iom->autoStop();
    return 1;
}

int test_condition_timer() {
    IOManager::ptr iom = std::make_shared<IOManager>("test_timer", 4);
    TimeManager::ptr tim = std::make_shared<TimeManager>(iom, 2000);
    auto begin = TimeManager::GetCurrentMS();
    M_SYLAR_LOG_INFO(g_logger) << "start test condition timer, time= " << begin;

    tim->init();


    std::atomic<int> condition_counter {0};
    std::atomic<int> loop_counter {0};
    for(int i = 0; i < 10000; i++) {
        TimeTask::ptr time_task = TimeTask::create(1, true, 
            [&condition_counter, &loop_counter](TimeTask::ptr task) -> Task<void> {
                condition_counter++;
                if(condition_counter % 5000 == 0) {
                    loop_counter++;
                    M_SYLAR_LOG_INFO(g_logger) << "finished " << loop_counter * 5000 << " timer tasks" << std::endl;
                }
                if(condition_counter > 100000) {
                    task->cancel();
                }
                co_return;
            }, 
            [](TimeTask::ptr task) -> Task<bool> {
                co_return true;
            }, 
            [](TimeTask::ptr task) -> Task<void> {
                co_return ;
            });
        tim->addConditionTimer(time_task);
    }   

    for(int i = 0 ; i < 2; i++) {
        tim->printInfo();
        sleep(3);
    }
    tim->close();
    M_SYLAR_LOG_INFO(g_logger) << "test condition timer finished, time(from" << begin << ")= " << TimeManager::GetCurrentMS();
    M_SYLAR_LOG_INFO(g_logger) << "SUMMARY: total finished" << condition_counter << " tasks";

    iom->autoStop();
    return 1;
}




int main() {
    std::string config_path = "/home/koddnty/user/projects/sylar/m_sylar/m_sylar/conf/basic.json";
    std::cout << "[LoggerManager init] config path: " << config_path << std::endl;
    m_sylar::ConfigManager::LoadJson(config_path, 0);


    // test_base_timer();

    test_condition_timer();
    return 0;
}