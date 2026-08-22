#include <catch2/catch_test_macros.hpp>
#include "tools/Snowflake.hpp"
#include "coroutine/corobase.h"
#include "coroutine/coro20/ioManager.h"
using namespace m_sylar;

static Snowflake<1387063029359> sf;
static std::vector<std::set<int64_t>> m_results;


Task<void, TaskBeginExecuter> cFunc(int idx){
    std::set<int64_t> result;
    for (int j = 0; j < 100000; j++) {
        int64_t id = sf.nextid();
        try {
            result.insert(id);
        }
        catch(std::exception& e) {
            std::cout << e.what() << std::endl;
        }
    }
    m_results[idx] = result;
    co_return ;
}

int unit_test_snowflak() {

    sf.init();


    IOManager iom("snow flak test" , 8);
    int nums = 100;
    m_results.resize(nums);
    for (int i = 0; i < nums; i++) {
        auto task = TaskCoro20::create_coro(std::bind(cFunc, i));
        iom.schedule(std::move(task));
    }

    sleep (10);
    iom.autoStop();
    std::set<int64_t> combine;
    bool dup = false;
    for (auto& s : m_results) {
        for (auto id : s)
            dup |= !combine.insert(id).second;
    }

    if (dup) {
        std::cout << "存在重复key" << std::endl;
        return -1;
    }
    return 0;
}



TEST_CASE("test precise timer", "[tools]")
{
    // 测试少量条件定时器任务
    REQUIRE(0 == unit_test_snowflak());
}
