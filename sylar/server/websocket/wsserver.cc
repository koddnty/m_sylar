#include "server/websocket/wsserver.hpp"
#include "protocol/websocket/WebSocket.h"
#include <random>

namespace m_sylar {
namespace websocket {

m_sylar::Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

Task<int> WsServer::handShake(http::HttpSession::ptr http_session) {
    // 完成握手，建立websocket连接
    // 请求判断
    std::shared_ptr<protocol::http::parser::request> request = http_session->getRequest();

    std::string request_str = request->raw();
    std::shared_ptr<WebSocket> ws {};
    WebSocketFrameType state = ws->parseHandshake((unsigned char*)request_str.c_str(), request_str.size());


    // 2. 构造并发送websocket响应
    if(state != WebSocketFrameType::OPENING_FRAME) {
        M_SYLAR_LOG_WARN(g_logger) << "handshake failed, invalid websocket request, state=" << state;
        co_return state;
    }
    std::string resp = ws->answerHandshake();
    int rt = co_await http_session->co_sendResp(resp);
    if(rt == -1) {
        M_SYLAR_LOG_WARN(g_logger) << "handshake failed, send response failed, errno:" << errno << " error:" << strerror(errno);
        co_return -1;
    }

    


    // 3. 将http_session升级为websocket session，维护连接状态
    IOManager::getInstance()->schedule(std::bind(&WsServer::handleClient, shared_from_this(), http_session->getSocket()));
    co_return 0;
}





Task<void, TaskBeginExecuter> WsServer::handleClient(Socket::ptr client) {
    // 外部http服务器已经完成握手升级协议，传入的client是一个websocket连接
    // session 构建
    WsSession::ptr session = std::make_shared<WsSession>(client);
    int code = 1000;
    std::string reason {"Normal Closure"};

    // 通信
    do
    {
        // 获取并处理请求报文
        int rt = co_await session->co_recvMessage();
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


        // 更新session信息
        if(!session->updateSession())       
        {
            M_SYLAR_LOG_DEBUG(g_logger) << "failed to update session message";
            co_return;
        }
        is_keep_alive = session->isKeep();


        // websocket握手处理，成功则进入websocket流程，失败则关闭连接
        if(m_enable_websocket && response_count == 0) {
            // 握手
            int ws_rt = co_await m_ws_server->handShake(session);
            if(ws_rt == 0) {
                
                co_return;
            }
            else { // 握手失败
                M_SYLAR_LOG_WARN(g_logger) << "websocket handshake failed, close connection. client:" << client->toString();
                co_return;
            } 
        }


        M_SYLAR_LOG_INFO(g_logger) << "uri : " << session->getRequest()->get_uri();
        co_await execHandler(session->getRequest()->get_uri(), session);         // 处理任务回调
        // session->getResponse()->updateHeader();

        co_await session->co_sendResp();                // 发送响应报文
        M_SYLAR_LOG_INFO(g_logger) << "is keep alive : " << is_keep_alive;
        response_count++;
    } while (is_keep_alive && response_count < stack_deep);     // 限制最大请求数，防止死循环


    // 断开
    co_await session->getHandler()->onClose(session, code, reason);
    co_return;
}



}
}