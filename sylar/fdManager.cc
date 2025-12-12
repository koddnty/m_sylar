#include "fdManager.h"
#include "hook.h"
#include <fcntl.h>
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




FdManager::FdManager()
{
    m_fdCtxs.resize(64);
}

FdManager::~FdManager()
{

}

}