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

bool  HttpSession::setResponse()
{

}

int HttpSession::recvRequest()
{
    uint64_t buffer_size = m_request_parser->getBufferSize();
    size_t offset = 0;
    ssize_t message_len = 0;
    int total_length = 0;
    while(true)
    {
        // 接受缓冲区信息
        message_len = m_socket->recv(m_buffer + offset, buffer_size - offset, 0);
        total_length += message_len;
        if(message_len == 0)
        {   // 连接关闭
            return 0;
        }
        else if(message_len == -1)
        {   // 错误
            M_SYLAR_LOG_WARN(g_logger) << "[httpSession] recv error, errorno:" << errno << " error:" << strerror(errno);
            return -1;
        }
        if(total_length > m_request_parser->getMaxReqSize())
        {
            M_SYLAR_LOG_WARN(g_logger) << "[httpSession] message is too long, total length=" << total_length;
            return -1;
        }

        // 处理接收到的信息
        offset = m_request_parser->execute(m_buffer, offset + message_len);
        if(m_request_parser->isError())
        { // 错误检测
            M_SYLAR_LOG_WARN(g_logger) << "[httpSession] Invalid http request, socket:" << m_socket->toString();
            return -1;
        }

        if(m_request_parser->isFinished())
        {   // 完成处理
            return total_length;
        }
        else if(offset == 0)
        {   // buffer太小且超限制
            M_SYLAR_LOG_WARN(g_logger) << "[httpSession] HTTP parsing stalled - possible malicious request";
            return -1;
        }
    }
    return 0;
}

int HttpSession::sendResp()
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
        int send_size = m_socket->send(buffer + offset, total_size - offset);
        if(send_size == -1 && errno == EPIPE)
        {
            break;
        }
        if(send_size == -1)
        {
            M_SYLAR_LOG_WARN(g_logger) << "send failed, errno:" << errno << " error:" << strerror(errno);
            return -1;
        }
        offset += send_size;
    }
    return total_size;
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

void HttpServer::handleClient(Socket::ptr client)
{
    bool is_keep_alive = false;
    M_SYLAR_LOG_INFO(g_logger) << "newClient, socket :";
    do
    {
        HttpSession::ptr session(new HttpSession(client));
        // 获取并处理请求报文
        int rt = session->recvRequest();
        if(rt == 0) {return; /* 连接关闭 */}
        else if(rt < 0)
        {
            M_SYLAR_LOG_WARN(g_logger) << "recv http request failed"
                                       << "\nclient:" << client->toString();
            return;
        } 
        // M_SYLAR_LOG_INFO(g_logger) << "request:\n" << *(session->getRequest());       

        // 更新session信息
        if(!session->updateSession())       
        {
            // session->getResponse()->setStatus(HttpStatus::INTERNAL_SERVER_ERROR);
            M_SYLAR_LOG_DEBUG(g_logger) << "failed to update session message";
            return;
        }
        is_keep_alive = session->isKeep();

        execHandler(session->getRequest()->getPath(), session);         // 处理回调
        session->sendResp();                // 发送响应报文
    } while (is_keep_alive);
    client->close();
}


}
}