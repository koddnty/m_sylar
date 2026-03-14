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
#include "http/httpServer.h"
#include "http/httpParser.h"
#include "http/tcpServer.h"
#include "basic/log.h"
#include "http/http.h"

namespace m_sylar
{
namespace http
{
static Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

HttpSession::HttpSession(Socket::ptr socket)
    : m_socket(socket)
{
    m_socket->setRecvTimeOut(m_request_parser->getRecvTimeOut());
    m_socket->setSendTimeOut(m_request_parser->getSendTimeOut());
    uint64_t buffer_size = m_request_parser->getBufferSize();
    m_response.reset(new HttpResponse());
    m_request_parser.reset(new HttpRequestParser());
    m_buffer = new char[buffer_size];
}

HttpSession::~HttpSession()
{
    delete[] m_buffer;
}

bool HttpSession::setResponse()
{
    return true;
}

Task<int> HttpSession::recvRequest()
{
    uint64_t buffer_size = m_request_parser->getBufferSize();
    // buffer_size = 20;
    size_t offset = buffer_size;
    ssize_t message_len = 0;
    int total_length = 0;
    while(true)
    {
        // 接受缓冲区信息
        // message_len = m_socket->recv(m_buffer + buffer_size - offset, offset, 0);
        message_len = co_await m_socket->recv(m_buffer, buffer_size, 0);
        total_length += message_len;
        // std::cout << "---=---=---total length = " << total_length << std::endl;
        // M_SYLAR_LOG_DEBUG(g_logger) << "message:\n" << std::string(m_buffer, message_len);
        if(message_len == 0)
        {   // 连接关闭
            co_return 0;
        }
        else if(message_len == -1)
        {   // 错误
            M_SYLAR_LOG_WARN(g_logger) << "[httpSession] recv error, errorno:" << errno << " error:" << strerror(errno);
            co_return -1;
        }
        if(total_length > m_request_parser->getMaxReqSize())
        {
            M_SYLAR_LOG_WARN(g_logger) << "[httpSession] message is too long, total length=" << total_length;
            co_return -1;
        }

        // 处理接收到的信息
        offset = m_request_parser->execute(m_buffer, total_length);
        if(m_request_parser->isError())
        { // 错误检测
            M_SYLAR_LOG_WARN(g_logger) << "[httpSession] Invalid http request, socket:" << m_socket->toString();
            co_return -1;
        }

        if(m_request_parser->isFinished())
        {   // 完成处理
            // M_SYLAR_LOG_DEBUG(g_logger) << "[httpSession] recv success, total length = " << total_length;
            co_return total_length;
        }
        else if(offset == total_length - buffer_size)
        {   // buffer太小且超限制
            M_SYLAR_LOG_WARN(g_logger) << "[httpSession] HTTP parsing stalled - possible malicious request";
            co_return -1;
        }
    }
    M_SYLAR_LOG_DEBUG(g_logger) << "[httpSession] recv success, total length = " << total_length;
    co_return total_length;
}

Task<int> HttpSession::sendResp()
{
    std::stringstream ss;
    m_response->updateAndDump(ss);
    // M_SYLAR_LOG_DEBUG(g_logger) << "contentLength:" << m_response->getHeader("Content-Length");
    std::string content = ss.str();
    const char* buffer = content.c_str();
    ssize_t total_size = content.size();
    ssize_t offset = 0;
    while(offset < total_size)
    {
        int send_size = co_await m_socket->send(buffer + offset, total_size - offset);
        if(send_size == -1 && errno == EPIPE)
        {
            break;
        }
        if(send_size == -1)
        {
            M_SYLAR_LOG_WARN(g_logger) << "send failed, errno:" << errno << " error:" << strerror(errno);
            co_return -1;
        }
        offset += send_size;
    }
    co_return total_size;
}

bool HttpSession::updateSession()
{   // recv 后更新session状态， 如keep-alive
    std::string connect_header = getRequest()->getHeader("connect", "keep-alive");
    if(connect_header == "keep-alive")
    {
        m_is_keep_alive = true;
    }
    else
    {
        m_is_keep_alive = false;
    }
    return true;
}

void HttpServer::registerUrl(const std::string& url, HandlerFunc cb, http::HttpMethod method)
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


void HttpServer::GET(const std::string& url, HandlerFunc cb)
{
    registerUrl(url, cb, HttpMethod::GET);
}

void HttpServer::POST(const std::string& url, HandlerFunc cb)
{
    registerUrl(url, cb, HttpMethod::POST);
}

void HttpServer::execHandler(const std::string& url, HttpSession::ptr session)
{
    auto request_pair = m_urls.find(url);
    if(request_pair == m_urls.end() ||
       session->getRequest()->getMethod() != request_pair->second.first)
    {   // 方法不匹配
        return;
    }
    request_pair->second.second(session);
}

Task<void, TaskBeginExecuter> HttpServer::handleClient(Socket::ptr client)
{
    bool is_keep_alive = false;
    M_SYLAR_LOG_INFO(g_logger) << "newClient, socket :" << *client;
    do
    {
        HttpSession::ptr session(new HttpSession(client));
        // 获取并处理请求报文
        int rt = co_await session->recvRequest();
        // std::cout << "already resume handleClient" << std::endl;
        if(rt == 0) {break; /* 连接关闭 */}
        else if(rt < 0)
        {
            M_SYLAR_LOG_WARN(g_logger) << "recv http request failed"
                                       << "\nclient:" << client->toString();
            break;
        } 
        // M_SYLAR_LOG_INFO(g_logger) << "request:\n" << *(session->getRequest());       

        // 更新session信息
        if(!session->updateSession())       
        {
            // session->getResponse()->setStatus(HttpStatus::INTERNAL_SERVER_ERROR);
            M_SYLAR_LOG_DEBUG(g_logger) << "failed to update session message";
            co_return;
        }
        is_keep_alive = session->isKeep();

        execHandler(session->getRequest()->getPath(), session);         // 处理回调
        co_await session->sendResp();                // 发送响应报文
    } while (is_keep_alive);
    client->close();
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
    std::cout << "---------------------------------------------" << std::endl;
    auto t = std::bind(&HttpServer::startAccept, std::dynamic_pointer_cast<HttpServer>(shared_from_this()), sock);
    getIomanager()->schedule(TaskCoro20::create_coro(t));
    co_return;
}


}
}