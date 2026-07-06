#include "basic/timer/timer.hpp"
#include <sys/timerfd.h>


using namespace m_sylar;

static m_sylar::Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

int test_base_timer() {
    IOManager iom {"test_timer", 1};
    n_TimeManager tim(&iom, 2000);
    auto begin = n_TimeManager::GetCurrentMS();
    M_SYLAR_LOG_INFO(g_logger) << "start test timer, time= " << begin;

    tim.init();
    
    for(int i = 0; i < 1; i++) {
        TimeTask::ptr time_task = TimeTask::create(1 + 50 * i, true, []() -> Task<bool> {
            M_SYLAR_LOG_INFO(g_logger) << "time task executed";
            co_return true;
        });
        tim.addTimer(time_task);
    }   

    sleep(10);
    M_SYLAR_LOG_INFO(g_logger) << "test timer finished, time(from" << begin << ")= " << n_TimeManager::GetCurrentMS();

    iom.autoStop();
    return 1;
}

int test_condition_timer() {
    IOManager iom {"test_condition_timer", 4};
    n_TimeManager::ptr tim = std::make_shared<n_TimeManager>(&iom, 2000);
    auto begin = n_TimeManager::GetCurrentMS();
    M_SYLAR_LOG_INFO(g_logger) << "start test condition timer, time= " << begin;

    tim->init();

    n_TimeManager::setInstance(tim);

    std::atomic<int> condition_counter {0};
    std::atomic<int> loop_counter {0};
    sleep(1);
    for(int i = 0; i < 10000; i++) {
        TimeTask::ptr time_task = TimeTask::create(1, true, [&condition_counter, &loop_counter]() -> Task<bool> {
            condition_counter++;
            if(condition_counter % 5000 == 0) {
                loop_counter++;
                std::cout << "finished " << loop_counter * 5000 << " timer tasks" << std::endl;
            }
            co_return true;
        }, []() -> Task<bool> {
            co_return true;
        }, []() -> Task<bool> {
            co_return true;
        });
        tim->addConditionTimer(time_task);
    }   

    for(int i = 0 ; i < 2; i++) {
        tim->printInfo();
        sleep(3);
    }
    tim->close();
    sleep(10);
    M_SYLAR_LOG_INFO(g_logger) << "test condition timer finished, time(from" << begin << ")= " << n_TimeManager::GetCurrentMS();

    iom.autoStop();
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