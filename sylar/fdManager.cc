#include "fdManager.h"
#include "hook.h"
#include <asm-generic/socket.h>
#include <fcntl.h>
#include <mutex>
#include <shared_mutex>
#include <sys/stat.h>

namespace m_sylar
{

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
        int flag = original_fcntl(m_fd, F_GETFL,0);
        if(!(flag & O_NONBLOCK))
        {
            original_fcntl(m_fd, F_SETFL, flag | O_NONBLOCK);
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


void FdCtx::setTimeout(int type, uint64_t time)
{
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
{
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

FdCtx::ptr FdManager::get(int fd, bool auto_create)
{
    std::shared_lock<std::shared_mutex> rlock (m_rwMutex);
    // 无需创建fdctx
    if(m_fdCtxs.size() <= fd)
    {
        if(auto_create == false)
        {
            return nullptr;
        }
    } 
    else
    {
        if(m_fdCtxs[fd])
        {
            return m_fdCtxs[fd];
        }
    }
    rlock.unlock();

    // 需创建fdctx
    FdCtx::ptr new_ctx (new FdCtx(fd));
    m_fdCtxs.resize(fd * 1.5);          // 扩容
    std::unique_lock<std::shared_mutex> wlock(m_rwMutex);
    m_fdCtxs[fd] = new_ctx;
    return new_ctx;
}

void FdManager::del(int fd)
{
    std::unique_lock<std::shared_mutex> wlock(m_rwMutex);
    if(m_fdCtxs.size() <= fd)
    {
        return;
    }
    m_fdCtxs[fd].reset();
}

}