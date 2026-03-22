#include "basic/address.h"
#include "http/tcpServer.h"
#include "http/httpServer.h"
#include "basic/log.h"
// #include "basic/ioManager.h"
#include "coroutine/corobase.h"
#include <iostream>
#include <memory>
#include <unistd.h>
#include <vector>

m_sylar::Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

int test(m_sylar::IOManager* iom)
{
    std::cout << "11" << std::endl;
    m_sylar::IPv4Address::ptr host_addr = std::dynamic_pointer_cast<m_sylar::IPv4Address>(m_sylar::Address::LookupAnyIPAddress("0.0.0.0"));
    host_addr->setPort(8033);
    // m_sylar::UnixAddress::ptr host_addr2 (new m_sylar::UnixAddress("/tmp/unix_addr"));

    // m_sylar::TcpServer::ptr server (new m_sylar::TcpServer());
    m_sylar::http::HttpServer::ptr server (new m_sylar::http::HttpServer(iom));


    std::vector<m_sylar::Address::ptr> addrs;
    std::vector<m_sylar::Address::ptr> fails;
    addrs.push_back(host_addr);
    // addrs.push_back(host_addr2);
    
    server->bind(addrs, fails);
    server->start();


    return 0;
}

int main(void)
{
    m_sylar::IOManager iom ("main", 1);
    test(&iom);
    sleep(1000);
    iom.stop();
    return 0;
}

