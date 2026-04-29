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

bool TcpServer::bind(Address::ptr addr, int num)
{   // if success, return true, or false.
    std::vector<std::pair<Address::ptr, int>> addrs;
    addrs.push_back({addr, num});
    std::vector<Address::ptr> failes;
    return bind(addrs, failes);
}

bool TcpServer::bind(std::vector<std::pair<Address::ptr, int>>& addrs_num, std::vector<Address::ptr>& failed)
{   // if all successed, return true. return false if one address failed
    bool rt = true;
    for(auto& addr : addrs_num)
    {
        for(int i = 0; i < addr.second; i++)
        {   // 单地址多次bind,使用REUSEPORT选项
            Socket::ptr sock = Socket::CreateTCP(addr.first);
            if(!sock->bind(addr.first))
            {
                M_SYLAR_LOG_ERROR(g_logger) << "bind failed, server name : " << m_name 
                                            << " errno : " << errno << " error : " << strerror(errno)
                                            << " \nlocal address :       " << sock->getLocalAddress()->toString()
                                            << " \nremote address :      " << addr.first->toString();

                failed.push_back(addr.first);
                rt = false;
                continue;
            }
            if(!sock->listen())
            {
                M_SYLAR_LOG_ERROR(g_logger) << "listen failed, server name : " << m_name 
                                            << " errno : " << errno << " error : " << strerror(errno)
                                            << " \naddr :        " << addr.first->toString()
                                            << " \nlocal address :       " << sock->getLocalAddress()->toString()
                                            << " \nremote address :       " << sock->getRemoteAddress()->toString();
                failed.push_back(addr.first);
                rt = false;
                continue;
            }

            M_SYLAR_LOG_INFO(g_logger) << "bind and listen successed, address : " << addr.first->toString()
                                    << "\n socket fd=" << sock->toString();
            m_sockets.push_back(sock);
        }
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

Task<void, TaskBeginExecuter> TcpServer::handleClient(Socket::ptr client)
{
    M_SYLAR_LOG_DEBUG(g_logger) << "new client";
    sleep(2);
    co_return;
}

Task<void, TaskBeginExecuter> TcpServer::startAccept(Socket::ptr sock)
{
    while(!isStop())
    {
        Socket::ptr client = co_await sock->accept();
        if(client)
        {
            // std::cout << "new client" << std::endl;
            auto t = std::bind(&TcpServer::handleClient, shared_from_this(), client);
            m_iomanager->schedule(TaskCoro20::create_coro(t));

            // auto t = [self = shared_from_this(), client]() -> Task<void, TaskBeginExecuter> {
                // co_await self->handleClient(client);  // ✅ 动态绑定
            // };
        }
        else 
        {
            M_SYLAR_LOG_WARN(g_logger) << "accept failed, errno : " << errno << " error : " << strerror(errno);
        }
    }
         
}



}