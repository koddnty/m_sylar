#include "http/tcpServer.h"
#include "basic/address.h"
// #include "basic/hook.h"
#include "coroutine/corobase.h"
// #include "basic/ioManager.h"
#include "basic/config.h"
#include "basic/log.h"
#include "basic/socket.h"
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <format>
#include <pthread.h>



namespace m_sylar
{

m_sylar::ConfigVar<uint64_t>::ptr g_tcpServer_read_timeout = 
    m_sylar::configManager::Lookup("http.tcpserver.timeout.read", (uint64_t)(2 * 60 * 1000000), "tcp server read timeOut to hold a connect");

static Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");    


TcpServer::TcpServer(IOManager* io_manager)
    : m_iomanager(io_manager), m_readtimeout(g_tcpServer_read_timeout->getValue())
    , m_name("m_sylar/1.0.0"), m_stop(true)
{

}

TcpServer::~TcpServer()
{
    for(auto& sock : m_sockets)
    {
        sock->close();
    }
    m_sockets.clear();
}

bool TcpServer::bind(Address::ptr addr)
{   // if success, return true, or false.
    std::vector<Address::ptr> addrs;
    addrs.push_back(addr);
    std::vector<Address::ptr> failes;
    return bind(addrs, failes);
}

bool TcpServer::bind(std::vector<Address::ptr>& addrs, std::vector<Address::ptr>& failed)
{   // if all successed, return true. return false if one address failed
    bool rt;
    for(auto& addr : addrs)
    {
        Socket::ptr sock = Socket::CreateTCP(addr);
        if(!sock->bind(addr))
        {
            M_SYLAR_LOG_ERROR(g_logger) << "bind failed, server name : " << m_name 
                                        << " errno : " << errno << " error : " << strerror(errno)
                                        << " \nlocal address :       " << sock->getLocalAddress()->toString()
                                        << " \nremote address :      " << addr->toString();

            failed.push_back(addr);
            rt = false;
            continue;
        }
        if(!sock->listen())
        {
            M_SYLAR_LOG_ERROR(g_logger) << "listen failed, server name : " << m_name 
                                        << " errno : " << errno << " error : " << strerror(errno)
                                        << " \naddr :        " << addr->toString()
                                        << " \nlocal address :       " << sock->getLocalAddress()->toString()
                                        << " \nremote address :       " << sock->getRemoteAddress()->toString();
            failed.push_back(addr);
            rt = false;
            continue;
        }

        M_SYLAR_LOG_INFO(g_logger) << "bind and listen successed, address : " << addr->toString()
                                   << "\n socket fd=" << sock->toString();
        m_sockets.push_back(sock);
    }
    return rt;
}

bool TcpServer::start()
{   // start tcp server
    if(!isStop())
    {
        return true;
    }
    m_stop = false;

    for(auto& sock : m_sockets)
    {
        auto t = std::bind(&TcpServer::startAccept, shared_from_this(), sock);
        m_iomanager->schedule(TaskCoro20::create_coro(t));
    }
    return true;
}

bool TcpServer::stop()
{
    if(isStop())
    {
        return true;
    }
    m_stop = true;

    for(auto& sock : m_sockets)
    {
        sock->close();
    }
    return true;
}

void TcpServer::setRtimeout(uint64_t)
{

}

void TcpServer::setName(std::string name)
{

}

void TcpServer::handleClient(Socket::ptr client)
{
    M_SYLAR_LOG_DEBUG(g_logger) << "new client";
    sleep(2);
}

Task<void, TaskBeginExecuter> TcpServer::startAccept(Socket::ptr sock)
{
    while(!isStop())
    {
        Socket::ptr client = co_await sock->accept();
        if(client)
        {
            std::cout << "new client" << std::endl;
            m_iomanager->schedule(std::bind(&TcpServer::handleClient, shared_from_this(), client));
        }
        else 
        {
            M_SYLAR_LOG_WARN(g_logger) << "accept failed, errno : " << errno << " error : " << strerror(errno);
        }
    }
         
}



}