#pragma once
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include "basic/singleton.h"
#include "basic/socket.h"
#include "basic/config.h"
#include "server/tcp/tcpServer.h"
#include "protocol/http/parser.hpp"
#include "protocol/http/request.hpp"
#include "protocol/http/response.hpp"
#include "server/common/session.hpp"
// #include "server/websocket/wsserver.hpp"

namespace m_sylar
{
namespace websocket
{
    class WsServer;
}

namespace http
{

/**
    @brief http回话内容管理
        同一会话多次请求时，仅存储一个请求和响应，新的请求覆盖旧的请求
        buffer缓冲区维护：
            1. recv时将数据放入buffer中，维护buffer的起始和结束位置
            2. buffer数据只有当无数据或者开始指针到达最大时重置，继续从recv中读取
*/
class HttpSession : public Session
{
public: 
    using ptr = std::shared_ptr<HttpSession>;
    using Request = protocol::http::parser::request;
    using Response = protocol::http::parser::response;
    HttpSession(Socket::ptr socket);
    ~HttpSession();

    inline Request::ptr getRequest() { return m_request;}      // 用户获取当前http请求
    inline Response::ptr getResponse() {return m_response;};    // 获得响应报文以修改
    inline bool isKeep() const {return m_is_keep_alive; }
    inline void setKeepAlive(bool v) { m_is_keep_alive = v; }  // 强制置长短连接，用户一般不需要调用这个函数，除非想强制设置长短连接

    bool setResponse();                 // 设置响应报文， 成功返回true;

    Task<int> co_recvRequest();                 // 接收一个http请求
    Task<int> co_sendResp();                     // 针对当前请求返回响应
    Task<int> co_sendResp(const std::string& resp);   // 直接发送响应字符串，成功返回发送字节数

    bool updateSession();               // recv后更新session信息，如connect和cookie



private:
    // HttpRequestParser::ptr m_request_parser;    
    // HttpResponse::ptr m_response;
    Request::ptr m_request;
    Response::ptr m_response;
    bool m_is_keep_alive = true;       // 长短连接
};



class HttpServer : public TcpServer
{
public:
    // using HandlerFunc = std::function<void(HttpSession::ptr)>;
    using HandlerFunc = Task<void>(*)(HttpSession::ptr);
    using ptr = std::shared_ptr<HttpServer>;

    HttpServer(IOManager* iomanager = IOManager::getInstance()) : TcpServer(iomanager){}
    ~HttpServer() {}

    void registerUrl(const std::string& url, HandlerFunc cb, protocol::http::HttpMethod method);

    bool start() override;

    // bool initWsSupport(std::shared_ptr<websocket::WsServer> ws_server = nullptr);// 初始化websocket支持，传入websocket server实例
    
    /**
        * @brief 注册GET和POST方法的url处理函数
    */
    void GET(const std::string& url, HandlerFunc cb);
    void POST(const std::string& url, HandlerFunc cb);


    // 业务处理函数， 由用户注册的url处理函数调用
    Task<void> execHandler(const std::string& url, HttpSession::ptr session);
private:
    Task<void, TaskBeginExecuter> handleClient(Socket::ptr client) override;
    Task<void, TaskBeginExecuter> startAccept(Socket::ptr sock) override;
    std::map<std::string, std::pair<protocol::http::HttpMethod, HandlerFunc>> m_urls;

    // websocket支持
    bool m_enable_websocket = false;     // 是否支持websocket协议
    std::shared_ptr<websocket::WsServer> m_ws_server{nullptr};     // websocket server实例

};

// using HttpMgr = Singleton<HttpServer>;
}
}