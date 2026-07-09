#include <algorithm>
#include <atomic>
#include <cerrno>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <ostream>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include "basic/log.h"
#include "httpServer.hpp"

// #include "server/websocket/wsserver.hpp"


namespace m_sylar
{
namespace http
{

static HttpServer* t_http_server = nullptr;  

static Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");
ConfigVar<uint32_t>::ptr g_http_recv_timeout = ConfigManager::LookUp("servers.http.timeout.recv", uint32_t(30), 0, "http server recv timeout");
ConfigVar<uint32_t>::ptr g_http_send_timeout = ConfigManager::LookUp("servers.http.timeout.send", uint32_t(30), 0, "http server send timeout");
ConfigVar<uint32_t>::ptr g_http_buffer_size = ConfigManager::LookUp("servers.http.limit.buffer_size", uint32_t(1024), 0, "http server parser buffer size");
ConfigVar<uint32_t>::ptr g_http_max_request_size  = ConfigManager::LookUp("servers.http.limit.max_request_size", uint32_t(10485760), 0, "http server parser buffer size");

HttpSession::HttpSession(Socket::ptr socket)
    : Session(socket)
{
    setRecvTimeOut(g_http_recv_timeout->getValue());
    setSendTimeOut(g_http_send_timeout->getValue());
    setBufferSize(g_http_buffer_size->getValue());
    m_response.reset(new WS_Response());
    m_request.reset(new WS_Request());
}

HttpSession::~HttpSession()
{}

bool HttpSession::setResponse()
{
    return true;
}




/**
    @brief http request 请求处理函数
        维护buffer缓冲区
    @return -1 表示较为重要的服务端错误
    @return -2 表示不重要的客户端或其他错误
*/

Task<int> HttpSession::co_recvRequest()
{
    uint64_t max_request_size = g_http_max_request_size->getValue();
    M_SYLAR_LOG_DEBUG(g_logger) << " max request size=" << max_request_size;
    char* buffer = nullptr;     // 输出参数，指向接收数据的起始位置，不要对buffer进行delete操作
    size_t total_length = 0;      // 累计接收字节数
    while(true){
        // 接受缓冲区信息
        // message_len = m_socket->recv(m_buffer + buffer_size - offset, offset, 0);
        int recv_len = co_await recvMessage(&buffer, -1);
        // 接收错误处理
        if(recv_len == 0)
        {   // 连接关闭
            co_return 0;
        }
        else if(recv_len < 0)
        {   // 错误
            M_SYLAR_LOG_WARN(g_logger) << "recv http request failed"
                                        << ", errno:" << errno
                                        << " error:" << strerror(errno)
                                        << "\nclient:" << getRequest();
            co_return -1;
        }

        // 接收信息处理
        int sloved = 0;
        try{
            sloved = m_request->consume(buffer, recv_len);
        } catch (std::exception &e) {
            std::cout << e.what() << std::endl;
            co_return -1;
        }
        consume(sloved);
        total_length += sloved;

        // 解析状态处理
        if(total_length > max_request_size)
        {
            M_SYLAR_LOG_WARN(g_logger) << "[httpSession] message is too long, total length=" << total_length << " max length=" << max_request_size;
            co_return -1;
        }

        if(m_request->ready()) {
            // 解析完成
            break;
        }
    }
    // 更新session信息
    if(!updateSession())       
    {
        M_SYLAR_LOG_WARN(g_logger) << "failed to update session message";
        co_return -1;
    }
    M_SYLAR_LOG_DEBUG(g_logger) << "[httpSession] recv success, total length = " << total_length;
    co_return total_length;
}

Task<int> HttpSession::co_sendResp()
{
    // 默认设置码
    if(m_response->get_status_code() == protocol::http::status_code::uninitialized)
    {
        m_response->set_status(protocol::http::status_code::ok);
    }
    m_response->set_version("HTTP/1.1");
    // header配置
    if(isKeep()) {
        m_response->append_header("Connection", "keep-alive");
    }
    else {
        m_response->append_header("Connection", "close");
    }

    std::string content = m_response->raw();
    co_return co_await co_sendResp(content);
    // M_SYLAR_LOG_INFO(g_logger) << "WS_Response content : \n" << content << std::endl;
    // const char* buffer = content.c_str();
    // ssize_t total_size = content.size();
    // ssize_t offset = 0;
    // while(offset < total_size)
    // {
    //     int send_size = co_await sendMessage(buffer + offset, total_size - offset);
    //     if(send_size == -1 && errno == EPIPE)
    //     {
    //         break;
    //     }
    //     if(send_size == -1)
    //     {
    //         // M_SYLAR_LOG_WARN(g_logger) << "send failed, errno:" << errno << " error:" << strerror(errno);
    //         co_return -1;
    //     }
    //     offset += send_size;
    // }
}

Task<int> HttpSession::co_sendResp(const std::string& resp) {
    // M_SYLAR_LOG_INFO(g_logger) << "WS_Response content : \n" << resp << std::endl;
    const char* buffer = resp.c_str();
    ssize_t total_size = resp.size();
    ssize_t offset = 0;
    while(offset < total_size)
    {
        int send_size = co_await sendMessage(buffer + offset, total_size - offset);
        if(send_size == -1 && errno == EPIPE)
        {
            break;
        }
        if(send_size == -1)
        {
            // M_SYLAR_LOG_WARN(g_logger) << "send failed, errno:" << errno << " error:" << strerror(errno);
            co_return -1;
        }
        offset += send_size;
    }
    co_return total_size;
}

bool HttpSession::updateSession()
{   // recv 后更新session状态， 如keep-alive
    // connection 头
    std::string connect_header = getRequest()->getHeader("Connection");
    if(connect_header == "close")
    {
        m_is_keep_alive = false;
    }


    return true;
}

void HttpServer::registerUrl(const std::string& url, HandlerFunc cb, protocol::http::HttpMethod method)
{
    m_urls[url] = {method, cb};
}

bool HttpServer::start()
{
    if(!isStop())
    {
        return true;
    }
    m_stop = false;

    for(auto& sock : getSockets())
    {
        auto t = std::bind(&HttpServer::startAccept, std::dynamic_pointer_cast<HttpServer>(shared_from_this()), sock);
        getIomanager()->schedule(TaskCoro20::create_coro(t));
    }
    return true;
}

// bool HttpServer::initWsSupport(websocket::WsServer::ptr ws_server) {
//     if(ws_server) {
//         m_ws_server = ws_server;
//     }
//     else {
//         m_ws_server = std::make_shared<websocket::WsServer>();
//     }
//     m_enable_websocket = true; 
//     return true; 
// }  


void HttpServer::GET(const std::string& url, HandlerFunc cb)
{
    registerUrl(url, cb, protocol::http::HttpMethod::GET);
}

void HttpServer::POST(const std::string& url, HandlerFunc cb)
{
    registerUrl(url, cb, protocol::http::HttpMethod::POST);
}

Task<void> HttpServer::execHandler(const std::string& url, HttpSession::ptr session)
{
    // M_SYLAR_LOG_INFO(g_logger) << "url: " << url;
    int idx = url.find("?");
    std::string path = url;
    if(idx != std::string::npos)
    {
        path = url.substr(0, idx);
    }
    M_SYLAR_LOG_INFO(g_logger) << "url: " << path;

    auto request_pair = m_urls.find(path);
    if(request_pair == m_urls.end() ||
       session->getRequest()->getMethod() != HttpMethodToString(request_pair->second.first))
    {   // 方法不匹配
        M_SYLAR_LOG_DEBUG(g_logger) << "no handler for url: " << path << " method: " << session->getRequest()->getMethod();
        co_return;
    }
    // request_pair->second.secownd(session);
    co_await request_pair->second.second(session);
    co_return;
}


HttpServer* HttpServer::getInstance() {
    if(t_http_server)
    {
        return t_http_server;
    }
    m_sylar::IOManager* iom = m_sylar::IOManager::getInstance();
    if(!iom)
    {
        M_SYLAR_LOG_ERROR(g_logger) << "getInstance failed, iom is nullptr";
        return nullptr;
    }
    
    static HttpServer httpserver(iom);
    t_http_server = &httpserver;
    if(t_http_server)
    {
        return t_http_server;
    }
    M_SYLAR_ASSERT2(false, "can not get http server without a iomanager");
}


Task<void, TaskBeginExecuter> HttpServer::handleClient(Socket::ptr client)
{
    bool is_keep_alive = false;
    // M_SYLAR_LOG_INFO(g_logger) << "newClient, socket :" << *client;   
    int response_count = 0;    
    int stack_deep = 1000;  
    HttpSession::ptr session(new HttpSession(client));

    do
    {
        // 获取并处理请求报文
        int rt = co_await session->co_recvRequest();
        // std::cout << "already resume handleClient" << std::endl;
        if(rt == 0) {break; /* 连接关闭 */}
        else if(rt == -1 && errno != EAGAIN){
            M_SYLAR_LOG_WARN(g_logger) << "recv http request failed"
                                        << ", errno:" << errno
                                        << " error:" << strerror(errno)
                                        << "\nclient:" << client->toString();
            break;
        }
        else if(rt == -2){
            break;
        }



        is_keep_alive = session->isKeep();


        // websocket握手处理，成功则进入websocket流程，失败则关闭连接
        // if(m_enable_websocket && response_count == 0) {
        //     // 握手
        //     int ws_rt = co_await m_ws_server->handShake(session);
        //     if(ws_rt == 0) {

        //         co_return;
        //     }
        //     else { // 握手失败
        //         M_SYLAR_LOG_WARN(g_logger) << "websocket handshake failed, close connection. client:" << client->toString();
        //         co_return;
        //     } 
        // }


        M_SYLAR_LOG_INFO(g_logger) << "uri : " << session->getRequest()->getUri();
        co_await execHandler(session->getRequest()->get()->get_uri(), session);         // 处理任务回调
        // session->getResponse()->updateHeader();
        M_SYLAR_LOG_DEBUG(g_logger) << "httpServer next loop";

        // co_await session->co_sendResp();                // 发送响应报文      // ！！！需要所有程序改动
        response_count++;
    } while (is_keep_alive && response_count < stack_deep);     // 限制最大请求数，防止死循环

    // 协程重调度
    if(is_keep_alive && response_count >= stack_deep)
    {
        M_SYLAR_LOG_WARN(g_logger) << "WS_Response count has reached the limit, close connection. client:" << client->toString();
        auto t = std::bind(&HttpServer::handleClient, std::dynamic_pointer_cast<HttpServer>(shared_from_this()), client);
        getIomanager()->schedule(TaskCoro20::create_coro(t));
    }
    else {
        client->close();
    }
    co_return;
}


Task<void, TaskBeginExecuter> HttpServer::startAccept(Socket::ptr sock)
{
    int count = 1000;
    while(!isStop() && --count)
    {
        Socket::ptr client = co_await sock->accept();       // 新连接到来时子任务resume后恢复，但子任务无法析构。
        if(client)
        {
            // std::cout << "new client" << std::endl;
            auto t = std::bind(&HttpServer::handleClient, std::dynamic_pointer_cast<HttpServer>(shared_from_this()), client);
            getIomanager()->schedule(TaskCoro20::create_coro(t));       // 为新连接注册任务
        }
        else 
        {
            M_SYLAR_LOG_WARN(g_logger) << "accept failed, errno : " << errno << " error : " << strerror(errno);
        }
    }
    // 重新调度accept
    // std::cerr << "-";
    auto t = std::bind(&HttpServer::startAccept, std::dynamic_pointer_cast<HttpServer>(shared_from_this()), sock);
    getIomanager()->schedule(TaskCoro20::create_coro(t));
    co_return;
}


}
}