#include "server/websocket/wsserver.hpp"
#include "protocol/websocket/WebSocket.h"
#include <random>

namespace m_sylar {
namespace websocket {

m_sylar::Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

ConfigVar<uint32_t>::ptr g_ws_recv_timeout = ConfigManager::LookUp("servers.websocket.timeout.recv", uint32_t(30), 0, "websocket server recv timeout");
ConfigVar<uint32_t>::ptr g_ws_send_timeout = ConfigManager::LookUp("servers.websocket.timeout.send", uint32_t(30), 0, "websocket server send timeout");
ConfigVar<uint32_t>::ptr g_ws_ping_timeout = ConfigManager::LookUp("servers.websocket.timeout.ping", uint32_t(30), 0, "websocket server send timeout");
ConfigVar<uint32_t>::ptr g_ws_buffer_size = ConfigManager::LookUp("servers.websocket.limit.buffer_size", uint32_t(1024), 0, "websocket server parser buffer size");
ConfigVar<uint32_t>::ptr g_ws_max_request_size  = ConfigManager::LookUp("servers.websocket.limit.max_request_size", uint32_t(10485760), 0, "websocket server parser buffer size");


Task<int> WsHandler::co_Route(std::shared_ptr<WsSession> session, Frame::ptr frame) {
    // 处理不同类型的帧
    int rt = 0;
    switch(frame->getType()) {
        case websocket_flags::WS_OP_TEXT:
            co_await co_onMessage(session, frame->getTextPayload());
            break;
        case websocket_flags::WS_OP_BINARY:
            co_await co_onBinary(session, frame->getBinaryPayload());
            break;
        case websocket_flags::WS_OP_CLOSE:
            co_await co_onClose(session, frame->getCloseCode(), frame->getCloseReason());   // 1000是正常关闭的状态码
            break;
        case websocket_flags::WS_OP_PING:
            co_await co_onPing(session, frame->getTextPayload());
            break;
        case websocket_flags::WS_OP_PONG:
            co_await co_onPong(session, frame->getTextPayload());
            break;
        default:
            co_await co_onError(session, "Unsupported frame type: " + std::to_string((int)frame->getType()));
            M_SYLAR_LOG_WARN(g_logger) << "Received frame with opcode: " << (int)frame->getType() << ", payload length: " << frame->getPayloadLength();
            rt = -1;
            break;
    }
    co_return rt;
}

Task<void> WsHandler::co_onOpen(std::shared_ptr<WsSession> session) {
    M_SYLAR_LOG_INFO(g_logger) << "unhandled websocket onOpen event, sessionId=" << session->getSessionId();
}

Task<void> WsHandler::co_onMessage(std::shared_ptr<WsSession> session, const std::string& msg) {
    M_SYLAR_LOG_INFO(g_logger) << "unhandled websocket onMessage event, sessionId=" << session->getSessionId();

}

Task<void> WsHandler::co_onBinary(std::shared_ptr<WsSession> session, const std::vector<uint8_t>& data) {
    M_SYLAR_LOG_INFO(g_logger) << "unhandled websocket onBinary event, sessionId=" << session->getSessionId();

}

Task<void> WsHandler::co_onClose(std::shared_ptr<WsSession> session, int code, const std::string& reason) {
    M_SYLAR_LOG_INFO(g_logger) << "unhandled websocket onClose event, sessionId=" << session->getSessionId();

}

Task<void> co_onPing(std::shared_ptr<WsSession> session, const std::string& reason) {
    // 收到ping消息，回复pong消息
    M_SYLAR_LOG_INFO(g_logger) << "Received ping message, sessionId=" << session->getSessionId() << ", reason=" << reason;
    Frame pongFrame{};
    pongFrame.setOpcode(websocket_flags::WS_OP_PONG);
    pongFrame.setPongPayload(reason);
    co_await session->co_sendFrame(pongFrame);
}
<void> co_onPing(std::shared_ptr<WsSession> session, const std::string& reason) {
    // 收到ping消息，回复pong消息
    M_SYLAR_LOG_INFO(g_logger) << "Received ping message, sessionId=" << session->getSessionId() << ", reason=" << reason;
    Frame pongFrame{};
    pongFrame.setOpcode(websocket_flags::WS_OP_PONG);
    pongFrame.setPongPayload(reason);
    co_await session->co_sendFrame(pongFrame);
}

Task<void> co_o
Task<void> co_onPong(std::shared_ptr<WsSession> session, const std::string& reason) {
}

Task<void> WsHandler::co_onError(std::shared_ptr<WsSession> session, const std::string& error) {
    M_SYLAR_LOG_INFO(g_logger) << "unhandled websocket onError event, sessionId=" << session->getSessionId();
}







WsSession::WsSession(Socket::ptr socket, size_t sessionId) : Session(socket) {
    setRecvTimeOut(g_ws_recv_timeout->getValue());
    setSendTimeOut(g_ws_send_timeout->getValue());
    setBufferSize(g_ws_buffer_size->getValue());
    m_sessionId = sessionId;
}


Task<int> WsSession::co_recvFrame() {
    if(m_frame_buffer.getFrameCount() > 0) {
        co_return 0;   // 已经有可用帧，无需继续接收
    }
    uint32_t max_request_size = g_ws_max_request_size->getValue();
    char* buffer = nullptr;     // 输出参数，指向接收数据的起始位置，不要对buffer进行delete操作
    size_t total_length = 0;      // 累计接收字节数

    while(true) {
        int rt = co_await recvMessage(&buffer, -1);
        if(rt == 0) {   // 连接关闭
            M_SYLAR_LOG_WARN(g_logger) << "recv websocket frame failed, buffer is null, close connection. client:" << getSocket()->toString();
            co_return 0;
        }
        else if(rt < 0) {  // 错误
            M_SYLAR_LOG_ERROR(g_logger) << "recv websocket frame failed, errno:" << errno << " error:" << strerror(errno) << "\nclient:" << getSocket()->toString();
            co_return -1;
        }


        // 帧解析
        total_length += rt;
        if(total_length > max_request_size) {
            M_SYLAR_LOG_WARN(g_logger) << "recv websocket frame failed, total_length=" << total_length << " exceeds max_request_size=" << max_request_size << ", close connection. client:" << getSocket()->toString();
            co_return -1;
        }
        int consumed = m_frame_buffer.consume((unsigned char*)buffer, rt);
        if(consumed < 0) {
            M_SYLAR_LOG_ERROR(g_logger) << "websocket frame parse error, close connection. client:" << getSocket()->toString();
            co_return -1;
        }

        consume(consumed);   // 更新缓冲区状态
        if(m_frame_buffer.getFrameCount() > 0) {
            break;  // 已经解析出完整帧，退出循环
        }
    }

    co_return 0;
}

Task<int> WsSession::co_sendFrame(const Frame& frame) {
    auto data = frame.make();
    co_await sendMessage((const char*)data.data(), data.size());
    co_return 0;
}

Task<int> WsSession::co_sendFrame(Frame::ptr frame) {
    auto data = frame->make();
    co_await sendMessage((const char*)data.data(), data.size());
    co_return 0;
}

Task<int> WsSession::co_close(int code, const std::string& reason) {
    Frame f{};
    f.setOpcode(websocket_flags::WS_OP_CLOSE);
    f.setClosePayload(code, reason);
    auto data = f.make();
    co_await sendMessage((const char*)data.data(), data.size());   // 发送空消息，触发底层连接关闭
    co_return 0;
}



WsServer::WsServer(http::HttpServer::ptr httpServer, int maxSession) {
    
}

WsServer::~WsServer() {
    
}

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
    co_return 0;
}





}
}

