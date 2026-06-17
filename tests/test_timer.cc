#include "basic/timer.hpp"
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

    iom.stop();
    return 1;
}

int test_condition_timer() {
    IOManager iom {"test_condition_timer", 1};
    n_TimeManager tim(&iom, 2000);
    auto begin = n_TimeManager::GetCurrentMS();
    M_SYLAR_LOG_INFO(g_logger) << "start test condition timer, time= " << begin;

    tim.init();
    std::atomic<int> condition_counter {0};

    for(int i = 0; i < 1; i++) {
        TimeTask::ptr time_task = TimeTask::create(1 + 50 * i, true, []() -> Task<bool> {
            std::cout << "1" << std::flush;
            co_return true;
        }, [&condition_counter]() -> Task<bool> {
            if(condition_counter++ % 2 == 0) {
                co_return true;
            }
            else {
                co_return false;
            }
        }, []() -> Task<bool> {
            std::cout << "0" << std::flush;
            co_return true;
        });
        tim.addConditionTimer(time_task);
    }   

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