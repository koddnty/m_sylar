#include "basic/log.h"
#include "http/http.h"
// #include "basic/hook.h"
#include "coroutine/corobase.h"
#include "basic/socket.h"
#include "basic/address.h"
#include <fstream>
#include <iostream>
#include <memory>
#include <ostream>
#include <sys/socket.h>

void test_req()
{
    m_sylar::http::HttpRequest::ptr req(new m_sylar::http::HttpRequest());
    req->setMethod(m_sylar::http::HttpMethod::POST);
    req->setHeader("Host", "baidu.com");
    req->setBody("Hello World");
    req->updateAndDump(std::cout) << std::endl;

    m_sylar::Socket::ptr sock (new m_sylar::Socket(AF_INET, SOCK_STREAM, 0));

    m_sylar::IPv4Address::ptr baidu(new m_sylar::IPv4Address("111.63.65.103", 80));

    std::cout << baidu->toString() << std::endl;

    sock->connect(baidu);

    std::ostringstream ss;
    req->updateAndDump(ss);
    std::string buffer = ss.str().c_str();
    sock->send(buffer.c_str(), buffer.size());

    char recvBuffer[1024] {'\0'};
    int length = 1024;
    sock->recv(recvBuffer, length);

    std::cout << recvBuffer << std::endl;
}

void test_resp()
{
    m_sylar::http::HttpResponse::ptr resp(new m_sylar::http::HttpResponse());
    resp->setBody("Hello World");
    resp->updateAndDump(std::cout) << std::endl;
}


int main(void)
{
    test_req();
    return 0;
}