#include "server/websocket/wsserver.hpp"
#include "protocol/websocket/WebSocket.h"
#include "basic/self_timer.h"
#include <random>

namespace m_sylar {
namespace websocket {

static thread_local WsServer* t_ws_server = nullptr;
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
    
    co_return;
}

Task<void> WsHandler::co_onMessage(std::shared_ptr<WsSession> session, const std::string& msg) {
    M_SYLAR_LOG_INFO(g_logger) << "unhandled websocket onMessage event, sessionId=" << session->getSessionId();
    co_return;
}

Task<void> WsHandler::co_onBinary(std::shared_ptr<WsSession> session, const std::vector<uint8_t>& data) {
    M_SYLAR_LOG_INFO(g_logger) << "unhandled websocket onBinary event, sessionId=" << session->getSessionId();
    co_return;
}

Task<void> WsHandler::co_onClose(std::shared_ptr<WsSession> session, int code, const std::string& reason) {
    // M_SYLAR_LOG_INFO(g_logger) << "unhandled websocket onClose event, sessionId=" << session->getSessionId();
    co_await session->co_close(code, reason);
    co_return;
}

Task<void> WsHandler::co_onPing(std::shared_ptr<WsSession> session, const std::string& reason) {
    // 收到ping消息，回复pong消息
    M_SYLAR_LOG_INFO(g_logger) << "Received ping message, sessionId=" << session->getSessionId() << ", reason=" << reason;
    Frame pongFrame{};
    pongFrame.setOpcode(websocket_flags::WS_OP_PONG);
    pongFrame.setPongPayload(reason);
    co_await session->co_sendFrame(pongFrame);
}

Task<void> WsHandler::co_onPong(std::shared_ptr<WsSession> session, const std::string& reason) {
    co_return;
}

Task<void> WsHandler::co_onError(std::shared_ptr<WsSession> session, const std::string& error) {
    M_SYLAR_LOG_INFO(g_logger) << "unhandled websocket onError event, sessionId=" << session->getSessionId();
    co_return;
}





// session分配器，连接列表等后续完善
WsSession::WsSession(Socket::ptr socket, size_t sessionId) : Session(socket) {
    setRecvTimeOut(g_ws_recv_timeout->getValue());
    setSendTimeOut(g_ws_send_timeout->getValue());
    setBufferSize(g_ws_buffer_size->getValue());
    m_sessionId = sessionId;

    m_state = State::INIT;
}

int WsSession::init() {
    auto self = shared_from_this();
    m_timer_fd = TimeManager::getInstance()->addConditionTimer(g_ws_ping_timeout->getValue(), true, 
        []() {      // 主循环，无需做任何事
        },
        [self](){       // 条件判断
            uint64_t recent = self->getRecentActivate();
            uint64_t now = m_sylar::TimeManager::GetCurrentMS();
            if(now - recent >= g_ws_ping_timeout->getValue()) {
                M_SYLAR_LOG_INFO(g_logger) << "ping timeout, close session, sessionId=" << self->getSessionId();
                return false;    // 超时，执行回调
            }
            return true;
        }, 
        [self]() {
            IOManager::getInstance()->schedule([self]() {
                self->m_state = State::CLOSED;
                WsServer::getInstance()->close(self->getSessionId());           // 关闭连接
                TimeManager::getInstance()->cancelTimer(self->m_timer_fd);
            });
        }
    );  
    if(m_timer_fd == -1) {
        M_SYLAR_LOG_ERROR(g_logger) << "failed to add ping timer for session, sessionId=" << m_sessionId;
        m_state = State::CLOSED;
        return -1;
    }
    m_state = State::OPEN;
    return 0;
}

Task<int> WsSession::co_recvFrame() {
    if(m_state != State::OPEN) {co_return -1;}
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

    co_return upDateSessionOnRecv();
}

Task<int> WsSession::co_sendFrame(const Frame& frame) {
    if(m_state != State::OPEN) {co_return -1;}
    auto data = frame.make();
    co_await sendMessage((const char*)data.data(), data.size());
    co_return 0;
}

Task<int> WsSession::co_sendFrame(Frame::ptr frame) {
    if(m_state != State::OPEN) {co_return -1;}
    auto data = frame->make();
    co_await sendMessage((const char*)data.data(), data.size());
    co_return 0;
}

Task<int> WsSession::co_close(int code, const std::string& reason) {
    if(m_state == State::CLOSED || m_state == State::CLOSING) {
        co_return 0;   // 已经在关闭中或已关闭，无需重复发送close帧
    }
    m_state = State::CLOSING;
    Frame f{};
    f.setOpcode(websocket_flags::WS_OP_CLOSE);
    f.setClosePayload(code, reason);
    auto data = f.make();
    co_await sendMessage((const char*)data.data(), data.size());   // 发送空消息
    // 关闭定时器
    if(m_timer_fd != -1) {
        TimeManager::getInstance()->cancelTimer(m_timer_fd);
        m_timer_fd = -1;
    }
    m_state = State::CLOSED;
    co_return 0;
}

int WsSession::upDateSessionOnRecv() {
    // 设置连接状态
    m_recent_activate = TimeManager::GetCurrentMS();   // 取消原有定时器
    return 0;
}






WsServer::WsServer(http::HttpServer::ptr httpServer, int maxSession) 
    : m_session_mutexs(maxSession) {
    m_httpServer = httpServer;
    m_sessions.resize(maxSession * 64);
    m_sessionIdAllocator = std::make_shared<PackedIDAllocator>(maxSession);
    for(size_t i = 0; i < m_session_mutexs.size(); i++) {
        m_session_mutexs[i] = std::make_unique<std::shared_mutex>();
    }
}

WsServer::WsServer(http::HttpServer* httpServer, int maxSession) 
    : m_session_mutexs(maxSession) {
    m_httpServer = std::shared_ptr<http::HttpServer>(httpServer, [](http::HttpServer*){});
    m_sessions.resize(maxSession * 64);
    m_sessionIdAllocator = std::make_shared<PackedIDAllocator>(maxSession);
    for(size_t i = 0; i < m_session_mutexs.size(); i++) {
        m_session_mutexs[i] = std::make_unique<std::shared_mutex>();
    }
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

int WsServer::close(int sessionId) {
    auto session = getSession(sessionId);
    if(!session) {
        M_SYLAR_LOG_ERROR(g_logger) << "close failed, session not found, sessionId=" << sessionId;
        return -1;
    }
    std::string reason {"Normal Closure"};
    return 0;
}


websocket::WsServer* websocket::WsServer::getInstance() {
    if(t_ws_server)
    {
        return t_ws_server;
    }
    auto http_server = http::HttpServer::getInstance();
    if(http_server == nullptr) {
        M_SYLAR_LOG_ERROR(g_logger) << "getInstance failed, http_server is nullptr";
        return nullptr;
    }
    
    static WsServer wsserver(http_server);
    t_ws_server = &wsserver;
    if(t_ws_server)
    {
        return t_ws_server;
    }
    M_SYLAR_ASSERT2(false, "can not get websocket server without a http server");
}  

WsSession::ptr WsServer::getSession(int sessionId) {
    if(sessionId < 0 || sessionId >= (int)m_sessions.size()) {
        M_SYLAR_LOG_FATAL(g_logger) << "getSession failed, sessionId(" << sessionId << ") is out of range, sessionId should be in range [0, " << m_sessions.size() - 1 << "]";
        return nullptr;
    }

    std::shared_lock<std::shared_mutex> r_lock(*m_session_mutexs[sessionId / 64]);
    return m_sessions[sessionId];
}

WsSession::ptr WsServer::createSession(Socket::ptr client) {
    int sessionId = m_sessionIdAllocator->alloc();
    if(sessionId < 0) {
        M_SYLAR_LOG_ERROR(g_logger) << "createSession failed, sessionId alloc failed, error code:" << (int)m_sessionIdAllocator->getErrorCode();
        return nullptr;
    }

    // 默认sessionId分配器是线程安全的，分配到的sessionId在当前时刻应该是唯一且未使用的，因此这里不需要加锁
    M_SYLAR_ASSERT2(m_sessions[sessionId] == nullptr, "sessionId=" + std::to_string(sessionId) + " is not free, session construct failed");

    // session 构建
    WsSession::ptr session = std::make_shared<WsSession>(client, sessionId);
    if(!session) {
        M_SYLAR_LOG_ERROR(g_logger) << "createSession failed, session construct failed, sessionId=" << sessionId;
        m_sessionIdAllocator->free(sessionId);   // 释放sessionId
        return nullptr;
    }

    // session初始化，注册心跳定时器等
    if(session->init() != 0) {
        M_SYLAR_LOG_ERROR(g_logger) << "createSession failed, session init failed, sessionId=" << sessionId;
        m_sessionIdAllocator->free(sessionId);   // 释放sessionId
        return nullptr;
    }
    
    // 将session添加到session列表
    std::unique_lock<std::shared_mutex> w_lock(*m_session_mutexs[sessionId / 64]);
    m_sessions[sessionId] = session;
    w_lock.unlock();
    // 完善统计信息
    m_sessionCount.fetch_add(1, std::memory_order_relaxed);
    return session;
}

int WsServer::removeSession(int sessionId) {
    if(sessionId < 0 || sessionId >= (int)m_sessions.size()) {
        M_SYLAR_LOG_ERROR(g_logger) << "createSession failed, sessionId alloc failed, error code:" << (int)m_sessionIdAllocator->getErrorCode();
        return -1;
    }

    std::unique_lock<std::shared_mutex> w_lock(*m_session_mutexs[sessionId / 64]);
    // 移除存储的session
    m_sessions[sessionId] = nullptr;

    // 释放sessionId
    m_sessionIdAllocator->free(sessionId);
    w_lock.unlock();
    // 完善统计信息
    m_sessionCount.fetch_sub(1, std::memory_order_relaxed);
    return 0;
}


}
}

