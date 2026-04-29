#include <cstdio>
#include <memory>
#include "basic/log.h"
#include "basic/address.h"
#include "http/httpServer.h"

static m_sylar::Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

m_sylar::Task<void> home_page(m_sylar::http::HttpSession::ptr session)
{
    session->getResponse()->setHeader("nihao", "110");
    std::string message = "hello world";
    session->getResponse()->setBody(message);
    co_return;
}

m_sylar::Task<void> rename_func(m_sylar::http::HttpSession::ptr session)
{
    session->getResponse()->setHeader("nihao", "110");
    std::string message = "没有改名卡口我";
    session->getResponse()->setBody(message);
    co_return;
}

void test_http_server(m_sylar::IOManager* iom)
{
    m_sylar::http::HttpServer::ptr server(new m_sylar::http::HttpServer(iom));
    m_sylar::Address::ptr addr = m_sylar::Address::LookupAnyIPAddress("0.0.0.0");
    std::dynamic_pointer_cast<m_sylar::IPv4Address>(addr)->setPort(8803);
    server->bind(addr, 6);
    
    server->GET("/home", home_page);

    server->POST("/home/rename", rename_func);
    server->start();
    M_SYLAR_LOG_INFO(g_logger) << "All Gate have been registered, ip:0.0.0.0:8803";
    sleep(1000);
    server->stop();
}

int main(void)
{
    m_sylar::IOManager iom("httpServer", 8);    
    // iom.schedule(test_http_server);
    test_http_server(&iom);
    // sleep(1000);
    iom.autoStop();
    return 0;
}