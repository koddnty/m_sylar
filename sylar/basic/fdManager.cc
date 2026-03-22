#include "basic/fdManager.h"
#include "basic/log.h"
#include "coroutine/corobase.h"
#include <asm-generic/socket.h>
#include <cstddef>
#include <fcntl.h>
#include <mutex>
#include <shared_mutex>
#include <sys/stat.h>

namespace m_sylar
{
static Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

FdCtx::FdCtx(int fd)
{
    m_is_init = false;
    m_is_closed = false;
    m_sysNoblock = false;
    m_userNoblock = false;
    m_is_socket = false;
    m_fd = fd;
    m_recvTimeout = -1;
    m_sendTimeout = -1;

    init();
}

FdCtx::~FdCtx()
{

}

bool FdCtx::init()  
{
    if(m_is_init)
    {
        return true;
    }
    m_recvTimeout = -1;
    m_sendTimeout = -1;

    // 检查fd状态
    struct stat fd_stat;
    if(-1 == fstat(m_fd, &fd_stat))
    {
        m_is_init = false;
        m_is_socket = false;
    }
    else
    {
        m_is_init = true;
        m_is_socket = S_ISSOCK(fd_stat.st_mode);
    }

    // 改变socketfd属性
    if(m_is_socket)
    {
        int flag = fcntl(m_fd, F_GETFL,0);
        if(!(flag & O_NONBLOCK))
        {
            fcntl(m_fd, F_SETFL, flag | O_NONBLOCK);
        }
        m_sysNoblock = true;
    }
    else 
    {
        m_sysNoblock = false;
    }

    m_userNoblock = false;
    m_is_closed = false;
    return m_is_init;
}

bool FdCtx::close()
{
    return true;
}


void FdCtx::setTimeout(int type, uint64_t time)
{   // usec
    if(type == SO_RCVTIMEO)
    {
        m_recvTimeout = time;
    }
    else
    {
        m_sendTimeout = time;
    }
}

// 无设置-1, 设置>0
uint64_t FdCtx::getTimeout(int type)
{   // usec
    if(type == SO_RCVTIMEO)
    {
        return m_recvTimeout;
    }
    else
    {
        return m_sendTimeout;
    }
}



FdManager::FdManager()
{
    m_fdCtxs.resize(64);
}

FdManager::~FdManager()
{

}


/**
    @brief 创建fdctx管理，根据auto_create决定是否创建fdctx

    @param fd 文件描述符

    @param auto_create 如果为真，则会自动创建未创建fdctx的fd。
*/
FdCtx::ptr FdManager::get(int fd, bool auto_create) 
{
    std::shared_lock<std::shared_mutex> rlock (m_rwMutex);
    // 无需创建fdctx
    // if(m_fdCtxs.size() <= fd)
    // {
    //     if(auto_create == false)
    //     {
    //         return nullptr;
    //     }
    // } 
    // else
    // {
    //     if(m_fdCtxs[fd])
    //     {
    //         return m_fdCtxs[fd];
    //     }
    // }

    if(!auto_create)
    {
        // if(fd == 6 && m_fdCtxs[6] == nullptr)
        // {
        //     // std::cout << "---------------------";
        // }
        return (fd < m_fdCtxs.size() ? m_fdCtxs[fd] : nullptr);
    }
    else if (fd < m_fdCtxs.size() && m_fdCtxs[fd])
    {  
        return m_fdCtxs[fd];
    }

    rlock.unlock();

    // 需创建fdctx
    // M_SYLAR_LOG_DEBUG(g_logger) << "create a new fdctx, fd=" << fd;
    FdCtx::ptr new_ctx (new FdCtx(fd));
    std::unique_lock<std::shared_mutex> wlock(m_rwMutex);
    if (fd < m_fdCtxs.size() && m_fdCtxs[fd]) 
    {   // 重新检查
        return m_fdCtxs[fd];
    }
    m_fdCtxs.resize(fd * 1.5);          // 扩容
    m_fdCtxs[fd] = new_ctx;
    return new_ctx;
}

void FdManager::del(int fd)
{
    // M_SYLAR_LOG_DEBUG(g_logger) << "delete a fd, fd=" << fd;
    std::unique_lock<std::shared_mutex> wlock(m_rwMutex);
    if(m_fdCtxs.size() <= fd)
    {
        return;
    }
    m_fdCtxs[fd].reset();
}

}