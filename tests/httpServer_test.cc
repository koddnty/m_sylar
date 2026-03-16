#include <cstdio>
#include <iostream>
#include <memory>
#include "basic/address.h"
// #include "basic/ioManager.h"
#include "coroutine/corobase.h"
#include "basic/log.h"
#include "http/tcpServer.h"
#include "http/httpServer.h"

static m_sylar::Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

void home_page(m_sylar::http::HttpSession::ptr session)
{
    session->getResponse()->setHeader("nihao", "110");
    std::string message = "hello world";
    session->getResponse()->setBody(message);
}

void rename_func(m_sylar::http::HttpSession::ptr session)
{
    session->getResponse()->setHeader("nihao", "110");
    std::string message = "没有改名卡口我";
    session->getResponse()->setBody(message);
}

void test_http_server(m_sylar::IOManager* iom)
{
    m_sylar::http::HttpServer::ptr server(new m_sylar::http::HttpServer(iom));
    m_sylar::Address::ptr addr = m_sylar::Address::LookupAnyIPAddress("0.0.0.0");
    std::dynamic_pointer_cast<m_sylar::IPv4Address>(addr)->setPort(8803);
    server->bind(addr);
    
    server->GET("/home", home_page);

    server->POST("/home/rename", rename_func);
    server->start();
    M_SYLAR_LOG_INFO(g_logger) << "All Gate have been registered, ip:0.0.0.0:8803";
    sleep(4000);
}

int main(void)
{
    m_sylar::IOManager iom("httpServer", 11);    
    // iom.schedule(test_http_server);
    test_http_server(&iom);
    // sleep(1000);
    iom.stop();
    return 0;
}