#include "coroutine/coro20/fiber.h"
#include "coroutine/coro20/ioManager.h"

namespace m_sylar
{

class IOExecuter : public AbstractExecuter
{
public:
    void execute(std::function<void ()> &&func) override    // 任务为控制func resme函数的调用时机
    {
        
    }
};
}