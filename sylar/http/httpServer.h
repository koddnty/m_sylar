#pragma once
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include "basic/singleton.h"
#include "http/http.h"
#include "basic/socket.h"
#include "http/tcpServer.h"
#include "http/httpParser.h"

namespace m_sylar
{
namespace http
{

/**
    @brief http回话内容管理
*/
class HttpSession
{
public: 
    using ptr = std::shared_ptr<HttpSession>;
    HttpSession(Socket::ptr socket);
    ~HttpSession();

    HttpRequest::ptr getRequest() { return m_request_parser->getData();}      // 用户获取当前http请求
    HttpResponse::ptr getResponse() {return m_response;};    // 获得响应报文以修改
    bool isKeep() {return m_is_keep_alive; }

    bool setResponse();                 // 设置响应报文， 成功返回true;

    Task<int> recvRequest();                 // 接收一个http请求
    Task<int> sendResp();                     // 针对当前请求返回响应

    bool updateSession();               // recv后更新session信息，如connect和cookie


private:
    Socket::ptr m_socket;               // 客户端通信socket
    HttpRequestParser::ptr m_request_parser;    
    HttpResponse::ptr m_response;
    char* m_buffer;
    bool m_is_keep_alive;       // 长短连接
};


class HttpServer : public TcpServer
{
public:
    using HandlerFunc = std::function<void(HttpSession::ptr)>;
    using ptr = std::shared_ptr<HttpServer>;

    HttpServer(IOManager* iomanager = IOManager::getInstance()) : TcpServer(iomanager){}
    ~HttpServer() {}

    void registerUrl(const std::string& url, HandlerFunc cb, http::HttpMethod method);

    bool start(int acceptNum = 1) override;
    
    void GET(const std::string& url, HandlerFunc cb);
    void POST(const std::string& url, HandlerFunc cb);


    void execHandler(const std::string& url, HttpSession::ptr session);
private:
    Task<void, TaskBeginExecuter> handleClient(Socket::ptr client) override;
    Task<void, TaskBeginExecuter> startAccept(Socket::ptr sock) override;
    std::map<std::string, std::pair<HttpMethod, std::function<void(HttpSession::ptr)>>> m_urls;
};

// using HttpMgr = Singleton<HttpServer>;
}
}