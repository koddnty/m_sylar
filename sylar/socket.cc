#include "socket.h"
#include "fdManager.h"
#include "sylar/log.h"
#include <asm-generic/socket.h>
#include <bits/types/error_t.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/tcp.h>



namespace m_sylar
{
static Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

Socket::Socket(int family, int type, int protocol)
{
    m_sock_fd = -1;
    m_family = family;
    m_type = type;
    m_protocol = protocol;
    m_isConnected = false;
}

Socket::~Socket()
{
    close();
}

int64_t Socket::getSendTimeOut()
{
    FdCtx::ptr fd_ctx = FdMgr::GetInstance()->get(m_sock_fd);
    if(fd_ctx)
    {
        return fd_ctx->getTimeout(SO_SNDTIMEO);
    }
    return -1;
}

void Socket::setSendTimeOut(int64_t time)
{
    FdCtx::ptr fd_ctx = FdMgr::GetInstance()->get(m_sock_fd);
    if(fd_ctx)
    {
        fd_ctx->setTimeout(SO_SNDTIMEO, time);
    }
}

int64_t Socket::getRecvTimeOut()
{
    FdCtx::ptr fd_ctx = FdMgr::GetInstance()->get(m_sock_fd);
    if(fd_ctx)
    {
        return fd_ctx->getTimeout(SO_RCVTIMEO);
    }
    return -1;
}

void Socket::setRecvTimeOut(int64_t time)
{
    FdCtx::ptr fd_ctx = FdMgr::GetInstance()->get(m_sock_fd);
    if(fd_ctx)
    {
        fd_ctx->setTimeout(SO_RCVTIMEO, time);
    }
}

bool Socket::getOption(int level, int option, void* result, socklen_t* len)
{
    int rt = getsockopt(m_sock_fd, level, option, result, len);
    if(rt)
    {
        M_SYLAR_LOG_DEBUG(g_logger) << "failed to get socket option, errno = " << errno 
                                    << "  error : " << strerror(errno)
                                    << "  socket fd : " << m_sock_fd
                                    << "  level     : " << level;
        return false;
    }
    return true;
}

bool Socket::setOption(int level, int option, const void* value, socklen_t len)
{
    int rt = setsockopt(m_sock_fd, level, option, value, len);
    if(rt)
    {
        M_SYLAR_LOG_DEBUG(g_logger) << "failed to get socket option, errno = " << errno 
                            << "  error : " << strerror(errno)
                            << "  socket fd : " << m_sock_fd
                            << "  level     : " << level;
        return false;
    }   
    return true;
}

Socket::ptr Socket::accept()
{
    Socket::ptr new_sock {new Socket(m_family, m_type, m_protocol)};
    int new_sock_fd = ::accept(m_sock_fd, nullptr, nullptr);
    if(new_sock_fd == -1)
    {
        M_SYLAR_LOG_ERROR(g_logger) << "accept failed, errno = " << errno
                                    << "   error : " << strerror(errno)
                                    << "   m_sock_fd = " << m_sock_fd;
        return nullptr;
    }
    if(new_sock->init(new_sock_fd)){
        return new_sock;
    }
    return nullptr;
}

bool Socket::init(int sock_fd)
{
    FdCtx::ptr fd_ctx = FdMgr::GetInstance()->get(m_sock_fd);
    if(fd_ctx && fd_ctx->is_socket() && !fd_ctx->is_closed())
    {
        m_sock_fd = sock_fd;
        m_isConnected = true;
        initSock();
        getLocalAddress();
        getRemoteAddress();
        return true;
    }
    return false;
}
         // create a Socket from socketFd
bool Socket::bind(const Address::ptr addr)
{

}

bool Socket::connect(const Address::ptr addr, uint64_t timeOut)
{

}

bool Socket::listen(int backlog)
{

}

bool Socket::close()
{

}

int Socket::send(const void* buffer, size_t length, int flags)
{

}

int Socket::send(const iovec* buffers, size_t length, int flags)
{

}

int Socket::sendTo(Address::ptr to, const void* buffer, size_t length, int flags)
{

}

int Socket::sendTo(Address::ptr to, const iovec* buffers, size_t length, int flags)
{

}

int Socket::recv(const void* buffer, size_t length, int flags)
{

}

int Socket::recv(const iovec* buffers, size_t length, int flags)
{

}

int Socket::recvFrom(Address::ptr from, const void* buffers, size_t length, int flags)
{

}

int Socket::recvFrom(Address::ptr from, const iovec* buffers, size_t length, int flags)
{

}

Address::ptr Socket::getRemoteAddress()
{

}

Address::ptr Socket::getLocalAddress()
{

}

std::ostream& Socket::dump(std::ostream& os) const
{
}
bool Socket::cancelRead()
{

}

bool Socket::cancelWrite()
{

}

bool Socket::cancelAll()
{

}

void Socket::initSock()
{
    int val = 1;
    setOption(SOL_SOCKET, SO_REUSEADDR, &val);          // set address reuse 
    if(m_type == SOCK_STREAM)
    {
        setOption(IPPROTO_TCP, TCP_NODELAY, &val);      // turn off nagle
    }
}

void Socket::newSock()
{
    m_sock_fd = socket(m_family, m_type, m_protocol);
    if(m_sock_fd != -1)
    {
        initSock();
    }
    
}

}