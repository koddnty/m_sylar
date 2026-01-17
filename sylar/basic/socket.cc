#include "socket.h"
#include "fdManager.h"
#include "address.h"
#include "ioManager.h"
#include "log.h"
#include <asm-generic/socket.h>
#include <bits/types/error_t.h>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <memory>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <sys/uio.h>
#include <unistd.h>



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

// easy create a socket 
Socket::ptr Socket::CreateTCP(m_sylar::Address::ptr address){
    Socket::ptr new_socket(new Socket(address->getFamily(), SOCK_STREAM, 0));
    return new_socket;
}

Socket::ptr Socket::CreateUDP(m_sylar::Address::ptr address){
    Socket::ptr new_socket(new Socket(address->getFamily(), SOCK_DGRAM, 0));
    return new_socket;
}

Socket::ptr Socket::CreateTCPSocket(){
    Socket::ptr new_socket(new Socket(AF_INET, SOCK_STREAM, 0));
    return new_socket;
}

Socket::ptr Socket::CreateUDPSocket(){
    Socket::ptr new_socket(new Socket(AF_INET, SOCK_DGRAM, 0));
    return new_socket;
}

Socket::ptr Socket::CreateTCPSocket6(){
    Socket::ptr new_socket(new Socket(AF_INET6, SOCK_STREAM, 0));
    return new_socket;
}

Socket::ptr Socket::CreateUDPSocket6(){
    Socket::ptr new_socket(new Socket(AF_INET6, SOCK_DGRAM, 0));
    return new_socket;
}

Socket::ptr Socket::CreateUnixTCPSocket(){
    Socket::ptr new_socket(new Socket(AF_UNIX, SOCK_STREAM, 0));
    return new_socket;
}

Socket::ptr Socket::CreateUnixUDPSocket(){
    Socket::ptr new_socket(new Socket(AF_UNIX, SOCK_DGRAM, 0));
    return new_socket;
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
{   // initialize a socket that created outside after connect or generate from accept
    FdCtx::ptr fd_ctx = FdMgr::GetInstance()->get(sock_fd);
    if(fd_ctx && fd_ctx->is_socket() && !fd_ctx->is_closed())
    {
        m_sock_fd = sock_fd;
        m_isConnected = true;
        initSock();             // init socket, set options
        getLocalAddress();
        getRemoteAddress();
        return true;
    }
    return false;
}

void Socket::initSock()
{   // basic init, set address reuse
    int val = 1;
    setOption(SOL_SOCKET, SO_REUSEADDR, &val);          // set address reuse 
    if(m_type == SOCK_STREAM)
    {
        setOption(IPPROTO_TCP, TCP_NODELAY, &val);      // turn off nagle
    }
}

void Socket::newSock()
{   // create a socket, mainly for 
    m_sock_fd = socket(m_family, m_type, m_protocol);
    if(m_sock_fd != -1)
    {
        initSock();
    }
}

bool Socket::bind(const Address::ptr addr)
{
    if(!isValid())
    {
        newSock();
        if(!isValid())
        {
            M_SYLAR_LOG_ERROR(g_logger) << "failed to create a new socket in socket::bind, errno = " << errno
                                        << "   e.what()" << strerror(errno);
            return false;
        }
    }
    if(addr->getFamily() != m_family)
    {
        M_SYLAR_LOG_ERROR(g_logger) << "family does not match between sockaddr(" << addr->getFamily() 
                                    << ") and Scoket.m_family(" << m_family << ")";
        return false;
    }

    if(::bind(m_sock_fd, addr->getAddr(), addr->getAddrLen()))
    {
        M_SYLAR_LOG_ERROR(g_logger) << "failed to bind socket, m_sock_fd = " << m_sock_fd 
                                    << "   local address : " << addr->toString()
                                    << "   errno = " << errno << "   e.what() : " << strerror(errno);
        return false;
    }
    // getLocalAddress();
    return true;
}

bool Socket::connect(const Address::ptr addr, uint64_t timeOut)
{
    if(!isValid())
    {
        newSock();
        if(!isValid())
        {
            M_SYLAR_LOG_ERROR(g_logger) << "failed to create a new socket in socket::bind, errno = " << errno
                                        << "   e.what()" << strerror(errno);
            return false;
        }
    }
    if(addr->getFamily() != m_family)
    {
        M_SYLAR_LOG_ERROR(g_logger) << "family does not match between sockaddr(" << addr->getFamily() 
                                    << ") and Scoket.m_family(" << m_family << ")";
        return false;
    }
    
    if(::connect(m_sock_fd, addr->getAddr(), addr->getAddrLen()))
    {
        M_SYLAR_LOG_ERROR(g_logger) << "failed to connect to (address)" << addr->toString()
                                    << "    errno = " << errno << "   error : " << strerror(errno);
        return false;
    }

    m_isConnected = true;
    getRemoteAddress();
    getLocalAddress();
    return true;
}

bool Socket::listen(int backlog)
{   // listen to a socket fd, generate a new connect socket when a connect is comming.
    if(!isValid())
    {
        M_SYLAR_LOG_ERROR(g_logger) << "socket is not valid in function Socket::listen(int)";
        return false;
    }
    if(::listen(m_sock_fd, backlog))
    {
        M_SYLAR_LOG_ERROR(g_logger) << "listen failed, m_sock_fd = " << m_sock_fd 
                                    << "    errno = " << errno << "   error : " << strerror(errno);
        return false;
    }
    return true;
}

bool Socket::close()
{
    if(!m_isConnected && m_sock_fd == -1)
    {
        return true;
    }
    m_isConnected = false;
    if(m_sock_fd != -1)
    {
        ::close(m_sock_fd);
        m_sock_fd = -1;
    }
    return false;
}

int Socket::send(const void* buffer, size_t length, int flags)
{
    if(m_isConnected)
    {
        return ::send(m_sock_fd, buffer, length, flags);
    }
    return -1;
}

int Socket::send(const iovec* buffers, size_t length, int flags)
{
    if(m_isConnected)
    {
        msghdr message {0};
        memset(&message, 0, sizeof(msghdr));
        message.msg_iov = (iovec*) buffers;
        message.msg_iovlen = length;
        return ::sendmsg(m_sock_fd, &message, flags);
    }
    return -1;
}

int Socket::sendTo(Address::ptr to, const void* buffer, size_t length, int flags)
{
    if(m_isConnected)
    {
        return ::sendto(m_sock_fd, buffer, length, flags, to->getAddr(), to->getAddrLen());
    }
    return -1;
}

int Socket::sendTo(Address::ptr to, const iovec* buffers, size_t length, int flags)
{
    if(m_isConnected)
    {
        msghdr message {0};
        memset(&message, 0, sizeof(msghdr));
        message.msg_name = to->getAddr();
        message.msg_namelen = to->getAddrLen();
        message.msg_iov = (iovec*) buffers;
        message.msg_iovlen = length;
        return ::sendmsg(m_sock_fd, &message, flags);
    }
    return -1;
}

int Socket::recv(const void* buffer, size_t length, int flags)
{
    if(m_isConnected)
    {
        return ::recv(m_sock_fd, (void*)buffer, length, flags);
    }
    return -1;
}

int Socket::recv(const iovec* buffers, size_t length, int flags)
{
    if(m_isConnected)
    {
        msghdr* message = new msghdr;
        memset(message, 0, sizeof(msghdr));
        message->msg_iov = (iovec*) buffers;
        message->msg_iovlen = length;
        return ::recvmsg(m_sock_fd, message, flags);
    }
    return -1;
}

int Socket::recvFrom(Address::ptr from, const void* buffers, size_t length, int flags)
{
    if(m_isConnected)
    {
        socklen_t len = from->getAddrLen();
        int status = ::recvfrom(m_sock_fd, (void*)buffers, length, flags, from->getAddr(), &len);
        if(from->getFamily() == AF_UNIX)
        {
            std::dynamic_pointer_cast<UnixAddress>(from)->setAddrLen(len);
        }
        return status;
    }
    return -1;
}

int Socket::recvFrom(Address::ptr from, const iovec* buffers, size_t length, int flags)
{
    if(m_isConnected)
    {
        msghdr message{0};
        memset(&message, 0, sizeof(msghdr));
        message.msg_name = from->getAddr();
        message.msg_namelen = from->getAddrLen();
        message.msg_iov = (iovec*) buffers;
        message.msg_iovlen = length;
        return ::recvmsg(m_sock_fd, &message, flags);
    }
    return -1;
}

Address::ptr Socket::getRemoteAddress()
{
    if(m_remote_address)
    {
        return m_remote_address;
    }

    Address::ptr result;    // nullptr, not initialized;

    switch(getFamily())         // set memory
    {
        case AF_INET:
            result.reset(new IPv4Address);
            break;
        case AF_INET6:
            result.reset(new IPv6Address);
            break;
        case AF_UNIX:
            result.reset(new UnixAddress);
            break;
        default:
            result.reset(new UnknownAddress);
    }

    socklen_t addrLen = result->getAddrLen();       // get remote address
    if(getpeername(m_sock_fd, result->getAddr(), &addrLen))
    {
        M_SYLAR_LOG_ERROR(g_logger) << "getpeername failed, errno = " << errno
                                    << "   error : " << strerror(errno);
        return nullptr;
    }

    if(result->getFamily() == AF_UNIX)      // correct UnixAddress length
    {
        std::dynamic_pointer_cast<UnixAddress>(result)->setAddrLen(addrLen);
    }

    m_remote_address = result;      
    return result;
}

Address::ptr Socket::getLocalAddress()
{
    if(m_remote_address)
    {
        return m_local_address;
    }

    Address::ptr result;    // nullptr, not initialized;

    switch(getFamily())         // set memory
    {
        case AF_INET:
            result.reset(new IPv4Address);
            break;
        case AF_INET6:
            result.reset(new IPv6Address);
            break;
        case AF_UNIX:
            result.reset(new UnixAddress);
            break;
        default:
            result.reset(new UnknownAddress);
    }

    socklen_t addrLen = result->getAddrLen();       // get remote address
    if(getsockname(m_sock_fd, result->getAddr(), &addrLen))
    {
        M_SYLAR_LOG_ERROR(g_logger) << "getsockname failed, errno = " << errno
                                    << "   error : " << strerror(errno);
        return nullptr;
    }

    if(result->getFamily() == AF_UNIX)      // correct UnixAddress length
    {
        std::dynamic_pointer_cast<UnixAddress>(result)->setAddrLen(addrLen);
    }

    m_local_address = result;      
    return result;

}

std::ostream& Socket::dump(std::ostream& os) const
{
    os  << "socket family : " << m_family 
        << "\naddress : " << m_local_address->toString()
        << "\nprotocol : " << m_protocol
        << "\nsocket fd : " << m_sock_fd
        << "\ntype : " << m_type;

    if(m_local_address)
    {
        os << "\n local address : \n" << m_local_address->toString();
    }
    if(m_remote_address)
    {
        os << "\n remote address : \n" << m_remote_address->toString();
    }
    return os;
}

bool Socket::cancelRead()
{
    return IOManager::getInstance()->cancelEvent(m_sock_fd, IOManager::READ);
}

bool Socket::cancelWrite()
{
    return IOManager::getInstance()->cancelEvent(m_sock_fd, IOManager::WRITE);
}

bool Socket::cancelAll()
{
    return IOManager::getInstance()->cancelAll(m_sock_fd);
}

bool Socket::isValid()
{
    return m_sock_fd != -1;
}


}