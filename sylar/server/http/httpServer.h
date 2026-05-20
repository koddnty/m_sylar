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

namespace m_sylar
{
namespace http
{

/**
    @brief http回话内容管理
        同一会话多次请求时，仅存储一个请求和响应，新的请求覆盖旧的请求
        buffer缓冲区维护：
            1. recv时将数据放入buffer中，维护buffer的起始和结束位置
            2. buffer数据只有当无数据或者开始指针到达最大时重置，继续从recv中读取
*/
class HttpSession
{
public: 
    using ptr = std::shared_ptr<HttpSession>;
    using Request = protocol::http::parser::request;
    using Response = protocol::http::parser::response;
    HttpSession(Socket::ptr socket);
    ~HttpSession();

    inline Request::ptr getRequest() { return m_request;}      // 用户获取当前http请求
    inline Response::ptr getResponse() {return m_response;};    // 获得响应报文以修改
    inline bool isKeep() {return m_is_keep_alive; }

    bool setResponse();                 // 设置响应报文， 成功返回true;

    Task<int> recvRequest();                 // 接收一个http请求
    Task<int> sendResp();                     // 针对当前请求返回响应

    bool updateSession();               // recv后更新session信息，如connect和cookie

    // class ReqBuffer {
    //     public: 
    //         ReqBuffer(size_t size) : m_size(size) {m_buffer = new char[size];}
    //         ~ReqBuffer() {delete[] m_buffer;}

    //         Task<int> recv(Socket::ptr socket) {
    //             ssize_t message_len = co_await socket->recv(m_buffer + m_end_pos, m_size - m_end_pos, 0);
    //             if(message_len >= 0)
    //             {
    //                 m_end_pos += message_len;
    //                 co_return message_len;
    //             }
    //             else
    //             {
    //                 co_return message_len;
    //             }
    //         }

    //         int read(char* buffer, int length) {
    //             if(buffer == nullptr) {return -1; }
    //             if(m_begin_pos + length > m_end_pos) {
    //                 return -1;
    //             }
    //             // 数据拷贝
    //             std::copy(m_buffer + m_begin_pos, m_buffer + m_begin_pos + length, buffer);
    //             m_begin_pos += length;
    //             // 数据移动
    //             if(m_begin_pos > m_size/2)
    //             {
    //                 int remain = m_end_pos - m_begin_pos;
    //                 std::copy(m_buffer + m_begin_pos, m_buffer + m_end_pos, m_buffer);
    //                 m_begin_pos = 0;
    //                 m_end_pos = remain;
    //             }
    //             return 0;
    //         }


    //     private:
    //         char* m_buffer;
    //         int m_size = 0;
    //         int m_begin_pos = 0;
    //         int m_end_pos = 0;
    // };


private:
    Socket::ptr m_socket;               // 客户端通信socket
    // HttpRequestParser::ptr m_request_parser;    
    // HttpResponse::ptr m_response;
    Request::ptr m_request;
    Response::ptr m_response;
    char* m_buffer;
    int m_buffer_begin_pos = 0;
    int m_buffer_end_pos = 0;
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
    
    void GET(const std::string& url, HandlerFunc cb);
    void POST(const std::string& url, HandlerFunc cb);


    Task<void> execHandler(const std::string& url, HttpSession::ptr session);
private:
    Task<void, TaskBeginExecuter> handleClient(Socket::ptr client) override;
    Task<void, TaskBeginExecuter> startAccept(Socket::ptr sock) override;
    std::map<std::string, std::pair<protocol::http::HttpMethod, HandlerFunc>> m_urls;
};

// using HttpMgr = Singleton<HttpServer>;
}
}