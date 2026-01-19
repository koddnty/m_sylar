#include <cerrno>
#include <cstdint>
#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
#include "http/httpSession.h"
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
    uint64_t buffer_size = m_request_parser->getBufferSize();
    m_response.reset(new HttpResponse());
    m_request_parser.reset(new HttpRequestParser());
    m_buffer = new char[buffer_size];
}

HttpSession::~HttpSession()
{

}

bool  HttpSession::setResponse()
{

}

bool HttpSession::recvRequest()
{
    uint64_t buffer_size = m_request_parser->getBufferSize();
    size_t offset = 0;
    ssize_t message_len = 0;
    bool rt = false;
    while(true)
    {
        // 接受缓冲区信息
        message_len = m_socket->recv(m_buffer + offset, buffer_size - offset, 0);
        if(message_len == 0)
        {
            M_SYLAR_LOG_WARN(g_logger) << "socket buffer is null";
            rt =  false;
            break;
        }
        else if(message_len == -1)
        {
            M_SYLAR_LOG_WARN(g_logger) << "recv error, errorno:" << errno << " error:" << strerror(errno);
            rt =  false;
            break;
        }
        // 处理接收到的信息
        offset = m_request_parser->execute(m_buffer, offset + message_len);
        if(m_request_parser->isError())
        { // 错误检测
            M_SYLAR_LOG_WARN(g_logger) << "[httpSession] Invalid http request, socket:" << m_socket->toString();
            rt = false;
            break;
        }

        if(m_request_parser->isFinished())
        {   // 完成处理
            rt = true;
            break;
        }
        else if(offset == 0)
        {   // buffer太小且超限制
            M_SYLAR_LOG_WARN(g_logger) << "HTTP parsing stalled - possible malicious request";
            rt = false;
            break;
        }
    }
    return rt;
}

int HttpSession::sendResp()
{

}

void HttpServer::handleClient(Socket::ptr client) 
{
    M_SYLAR_LOG_INFO(g_logger) << "newClient, socket :" << client->toString();
    HttpSession::ptr session(new HttpSession(client));
    session->recvRequest();
    M_SYLAR_LOG_INFO(g_logger) << "request:" << *(session->getRequest());
    sleep(2);
}

}
}