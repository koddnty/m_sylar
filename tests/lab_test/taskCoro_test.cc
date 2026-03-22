#include "coroutine/corobase.h"


using namespace m_sylar;

class BadAwaiter : public Awaiter<void>
{
public:
    void on_suspend() override
    {
    }
};

Task<void, TaskBeginExecuter> test_coro()
{
    co_await BadAwaiter();
    co_return ;
}


void test1()
{
    TaskCoro20 t(TaskCoro20::create_coro(test_coro));
    t.start();
    sleep(1);
}

int main(void)
{
    test1();
    return 0;
}