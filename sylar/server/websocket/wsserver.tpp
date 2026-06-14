#include "wsserver.hpp"

namespace m_sylar
{
namespace websocket
{

template<WsHandlerType T>
Task<int> WsHandler::co_Route(std::shared_ptr<WsSession> session, Frame::ptr frame) {
    // 处理不同类型的帧
    if(frame == nullptr) {
        M_SYLAR_LOG_ERROR(ghws_logger) << "frame is nullptr, sessionId=" << session->getSessionId();
        co_return -1;
    }
    int rt = 0;
    switch(frame->getType()) {
        case websocket_flags::WS_OP_TEXT:
            M_SYLAR_LOG_DEBUG(ghws_logger) << "WS_OP_TEXT";
            co_await T::co_onMessage(session, frame->getTextPayload());
            break;
        case websocket_flags::WS_OP_BINARY:
            M_SYLAR_LOG_DEBUG(ghws_logger) << "WS_OP_BINARY";
            co_await T::co_onBinary(session, frame->getBinaryPayload());
            break;
        case websocket_flags::WS_OP_CLOSE:
            M_SYLAR_LOG_DEBUG(ghws_logger) << "WS_OP_CLOSE";
            co_await T::co_onClose(session, frame->getCloseCode(), frame->getCloseReason());   // 1000是正常关闭的状态码
            break;
        case websocket_flags::WS_OP_PING:
            M_SYLAR_LOG_DEBUG(ghws_logger) << "WS_OP_PING";
            co_await T::co_onPing(session, frame->getTextPayload());
            break;
        case websocket_flags::WS_OP_PONG:
            M_SYLAR_LOG_DEBUG(ghws_logger) << "WS_OP_PONG";
            co_await T::co_onPong(session, frame->getTextPayload());
            break;
        default:
            M_SYLAR_LOG_DEBUG(ghws_logger) << "UNKNOWD OPCODE: " << (int)frame->getType();
            co_await T::co_onError(session, "Unsupported frame type: " + std::to_string((int)frame->getType()));
            M_SYLAR_LOG_WARN(ghws_logger) << "Received frame with opcode: " << (int)frame->getType() << ", payload length: " << frame->getPayloadLength();
            rt = -1;
            break;
    }
    co_return rt;
}

template<WsHandlerType T>
Task<int> WsServer::handleClient(Socket::ptr client, int sessionId) {
    // 外部http服务器已经完成握手升级协议，传入的client是一个websocket连接
    int code = 1000;
    int loopCount = 0;
    bool isClosed = false;
    std::string reason {"Normal Closure"};
    WsSession::ptr session{};
    if(sessionId < 0) {
        M_SYLAR_LOG_DEBUG(ghws_logger) << "handle websocket client, socket:" << *client;
        session = createSession(client);
        if(!session) {
            M_SYLAR_LOG_ERROR(ghws_logger) << "handleClient failed, create session failed, error code:" << (int)m_sessionIdAllocator->getErrorCode();
            co_return -1;    // 创建session失败，无法处理连接，直接关闭连接
        }
        sessionId = session->getSessionId();
        if(sessionId < 0) {
            M_SYLAR_LOG_ERROR(ghws_logger) << "handleClient failed, create session failed, error code:" << (int)m_sessionIdAllocator->getErrorCode();
            co_return -1;
        }

        co_await T::co_onOpen(session);    // 连接建立事件
    }
    else {
        session = getSession(sessionId);
        if(!session) {
            M_SYLAR_LOG_ERROR(ghws_logger) << "handleClient failed, session not found, sessionId=" << sessionId;
            co_return -1;    // session不存在，无法处理连接，直接关闭连接
        }
    }

    M_SYLAR_LOG_DEBUG(ghws_logger) << "websocket client connected, sessionId=" << sessionId << ", socket:" << *client;
    int nextId = sessionId;
    M_SYLAR_LOG_DEBUG(ghws_logger) << "new websocket client, sessionId=" << sessionId << ", socket:" << *client;
    // 通信
    do {
        loopCount++;
        // 获取报文
        int rt = co_await session->co_recvFrame();
        // std::cout << "already resume handleClient" << std::endl;
        M_SYLAR_LOG_DEBUG(ghws_logger) << "recv frame, sessionId=" << sessionId << ", rt=" << rt;
        if(rt == 0) {
            nextId = -1;
            break; /* 连接关闭 */
        }
        else if(rt == -1){
            M_SYLAR_LOG_WARN(ghws_logger) << "recv http request failed"
                                        << ", errno:" << errno
                                        << " error:" << strerror(errno)
                                        << "\nclient:" << client->toString();
            nextId = -1;
            break;
        }
        else if(rt == -2){
            nextId = -1;
            M_SYLAR_LOG_WARN(ghws_logger) << "recv http request failed, close connection. client:" << client->toString();
            break;
        }


        // 处理报文
        Frame::ptr f = session->getFrame();
        if(f == nullptr) {
            M_SYLAR_LOG_ERROR(ghws_logger) << "get frame failed(it should not be nullptr), sessionId=" << sessionId
                    << ", client:" << client->toString() << " rt = " << rt;

            nextId = -1;
            break;
        }

        M_SYLAR_LOG_DEBUG(ghws_logger) << "recv frame, sessionId=" << sessionId << ", opcode=" << (int)f->getType() << ", payload length=" << f->getPayloadLength();
        int state = co_await T::template co_Route<T>(session, f);

        if(state) {
            M_SYLAR_LOG_DEBUG(ghws_logger) << "handler return false, close connection. client:" << client->toString();
            code = 1002;  // 协议错误
            reason = "Protocol Error";
            rt = -1;
            co_await session->co_close(code, reason);
            break;
        }
    } while (loopCount < 10000 && session->getState() != WsSession::State::CLOSED);     // 限制最大请求数，防止死循环

    // M_SYLAR_LOG_DEBUG(ghws_logger) << "websocket client loop end, nextId=" << nextId << ", socket:" << *client << ", code=" << code << ", reason=" << reason;
    if(nextId == -1) {
        co_await session->co_close(code, reason);    // 确保连接关闭
        removeSession(sessionId);
    }
    // 断开/重新调度
    M_SYLAR_LOG_DEBUG(ghws_logger) << "websocket client one loop finished, isClosed " << isClosed 
                << "\n loopCount: " << loopCount << " state:" << (int)session->getState() 
                <<  "\n sessionId=" << sessionId << ", socket:" << *client << ", code=" << code << ", reason=" << reason;
    co_return nextId;
}


template<WsHandlerType T>
void WsServer::registerUrl(const std::string& url){
        static_assert(std::is_base_of_v<WsHandler, T>, "T must inherit from WsHandler");
        m_httpServer->registerUrl(url, [](http::HttpSession::ptr session) -> Task<void> {
            M_SYLAR_LOG_DEBUG(ghws_logger) << "receive websocket handshake request, url:" << session->getRequest()->get_uri();
            session->setKeepAlive(false);    // websocket连接不支持长连接，强制设置为短连接

            int rt = co_await WsServer::getInstance()->handShake(session);                          // 完成握手，建立websocket连接
            if(rt != 0) {
                M_SYLAR_LOG_WARN(ghws_logger) << "handshake failed, close connection. client:" << session->getSocket()->toString();
                co_return;
            }
            
            int sessionId = -1;
            do {
                M_SYLAR_LOG_DEBUG(ghws_logger) << "enter websocket handle loop, url:" << session->getRequest()->get_uri();
                sessionId = co_await WsServer::getInstance()->handleClient<T>(session->getSocket(), sessionId);          // 进入websocket流程处理
            } while(sessionId >= 0);
        }, protocol::http::HttpMethod::GET);   // websocket握手协议必须是GET方法
    }
}



}


