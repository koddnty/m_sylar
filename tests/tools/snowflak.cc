#include "tools/snowflak.hpp"
#include "coroutine/corobase.h"
#include "coroutine/coro20/ioManager.h"
using namespace m_sylar;

static snowflake<1387063029359> sf;
static std::vector<std::set<int64_t>> m_results;


Task<void, TaskBeginExecuter> cFunc(int idx){
    std::cout << idx << std::endl;
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

int main() {

    sf.init();
    // func();
    // return 0 ;

    IOManager iom("snow flak test" , 1);
    int nums = 100;
    m_results.resize(nums);
    for (int i = 0; i < nums; i++) {
        auto task = TaskCoro20::create_coro(std::bind(cFunc, i));
        iom.schedule(std::move(task));
    }

    sleep (10);
    std::set<int64_t> combine;
    bool dup = false;
    std::cout << "======\n";
    for (auto& s : m_results) {
        std::cout << s.size() << std::endl;
        for (auto id : s)
            dup |= !combine.insert(id).second;
    }

    std::cout << combine.size() << std::endl;
    if (dup) std::cout << "重复的id" << std::endl;
    iom.autoStop();
    return 0;
}
