#pragma once
#include <memory>
#include "http/http.h"
#include "basic/socket.h"
#include "http/tcpServer.h"
#include "http/httpParser.h"

namespace m_sylar
{
namespace http
{

class HttpSession
{
public: 
    using ptr = std::shared_ptr<HttpSession>;
    HttpSession(Socket::ptr socket);
    ~HttpSession();

    HttpRequest::ptr getRequest() { return m_request_parser->getData();}      // 用户获取当前http请求
    HttpResponse::ptr getResponse() {return m_response;};    // 获得响应报文以修改

    bool setResponse();                 // 设置响应报文， 成功返回true;

    bool recvRequest();                 // 接收一个http请求
    int sendResp();                     // 针对当前请求返回响应

private:
    Socket::ptr m_socket;
    HttpRequestParser::ptr m_request_parser;
    HttpResponse::ptr m_response;
    char* m_buffer;
};


class HttpServer : public TcpServer
{
public:
    HttpServer(IOManager* iomanager = IOManager::getInstance()) : TcpServer(){}
    ~HttpServer() {}

private:
    void handleClient(Socket::ptr client) override;
};

}
}