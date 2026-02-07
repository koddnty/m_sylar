#include "fiber.h"
#include "basic/log.h"
#include "basic/macro.h"
#include <functional>

namespace m_sylar
{
static m_sylar::Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

TaskCoro20::TaskCoro20(std::function<Task<>()> cb)
{
    M_SYLAR_ASSERT2(cb, "[TaskCoro20] bad task, failed to task");
    m_task = cb;
    m_status = Status::INIT;
}

TaskCoro20::TaskCoro20(std::function<void()> cb)
{
    
}


TaskCoro20::TaskCoro20(HandlePtr handle)
{
    M_SYLAR_ASSERT2(handle, "[TaskCoro20] bad task, failed to task");
    m_handler = handle;
    m_status = Status::SUSPEND;
}

TaskCoro20::~TaskCoro20()
{
}

void TaskCoro20::setTask(std::function<Task<>()> cb)
{
    M_SYLAR_ASSERT2(cb, "[TaskCoro20] bad task, failed to task");
    m_task = cb;
    m_status = INIT;
}

void TaskCoro20::setHandle(HandlePtr handle)
{
    M_SYLAR_ASSERT2(handle, "[TaskCoro20] bad handle, failed to setHandle");
    m_handler = handle;
    if(handle->done())
    {
        m_status = TERM;
    }
    else
    {
        m_status = SUSPEND;
    }
}

void TaskCoro20::reset()
{
    m_status = UNSET;
    m_task = nullptr;
    m_handler = nullptr;
}

void TaskCoro20::resume()
{
    if(m_status == Status::INIT)
    {
        M_SYLAR_ASSERT2(m_status == Status::INIT && !m_handler,
            "Invalid state: Expected status INIT with a unexcepted handler, "
            "but got status=%d and handler=%p");
        // FiberFunc(m_task);
        m_task();
        if(m_handler && !m_handler->done())
        {
            m_status = Status::SUSPEND;
        }
    }
    else if(m_status == Status::SUSPEND)
    {
        M_SYLAR_ASSERT2(m_handler, "[TaskCoro20]Invalid state: Expected status SUSPEND with no handler");
        m_handler->resume();
        m_status = EXEC;
    }
    else
    {
        M_SYLAR_LOG_ERROR(g_logger) << "[TaskCoro20] failed to resume a task, with status:" << m_status;
    }
}

bool TaskCoro20::isLegal()
{   // 如果任务是有效任务返回true

    switch(m_status)
    {
        case UNSET:
            return false;
            break;
        case INIT:
            return !!m_task;
            break;
        case SUSPEND:
            return !!m_handler;
            break;
        case EXEC:
            return !!m_handler;
            break;
        case TERM:
            return false;
            break;
        default:
            return false;


    }
}



// 协程封装函数
// Task<> TaskCoro20::FiberFunc(std::function<Task<>()> cb)
// {
//     cb();
//     // 运行完毕进行清理
//     if(m_handler)
//     {
//         m_handler->destroy();
//     }
//     co_return;
// }


}