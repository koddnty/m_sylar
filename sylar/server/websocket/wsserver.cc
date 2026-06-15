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
ConfigVar<uint32_t>::ptr g_ws_ping_interval = ConfigManager::LookUp("servers.websocket.interval.ping", uint32_t(5), 0, "websocket server send timeout");
ConfigVar<uint32_t>::ptr g_ws_pong_timeout = ConfigManager::LookUp("servers.websocket.timeout.pong", uint32_t(15), 0, "websocket server send timeout");
ConfigVar<uint32_t>::ptr g_ws_buffer_size = ConfigManager::LookUp("servers.websocket.limit.buffer_size", uint32_t(1024), 0, "websocket server parser buffer size");
ConfigVar<uint32_t>::ptr g_ws_max_request_size  = ConfigManager::LookUp("servers.websocket.limit.max_request_size", uint32_t(10485760), 0, "websocket server parser buffer size");


// Task<int> WsHandler::co_Route(std::shared_ptr<WsSession> session, Frame::ptr frame) 

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
    // 更新 最近pong时间戳
    
    session->setRecentPong(TimeManager::GetCurrentMS());
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

WsSession::~WsSession() {
}

int WsSession::init() {
    m_recent_pong = TimeManager::GetCurrentMS();
    auto self = shared_from_this();
    m_timer_fd = TimeManager::getInstance()->addConditionTimer(g_ws_ping_interval->getValue() * 1000000, true, 
        []()->Task<bool> {      // 主循环，无需做任何事
            M_SYLAR_LOG_DEBUG(g_logger) << "end interval of websocket Timer";
            co_return true;
        },
        [self]()->Task<bool> {       // 条件判断
            // 获取ping帧时间戳，判断是否超时
            uint64_t now = TimeManager::GetCurrentMS();
            // 发送ping
            Frame::ptr pingFrame = std::make_shared<Frame>();
            pingFrame->setOpcode(websocket_flags::WS_OP_PING);
            pingFrame->setPingPayload(std::to_string(now));
            co_await self->co_sendFrame(pingFrame);

            co_await co_sleep(g_ws_pong_timeout->getValue());   // 等待pong超时
            M_SYLAR_LOG_DEBUG(g_logger) << "ping check, sessionId=" << self->getSessionId();
            
            // 检查最近pong帧时间戳，如果超过超时时间，认为连接异常，进入关闭流程
            now = TimeManager::GetCurrentMS();
            uint64_t recent_pong = self->getRecentPong();
            if(now < recent_pong) {
                M_SYLAR_LOG_WARN(g_logger) << "current time is smaller than recent pong time, something may be wrong, sessionId=" << self->getSessionId();
                co_return true;    // 时间异常但不认为连接异常，继续等待
            }
            if(now > recent_pong && now - recent_pong > g_ws_pong_timeout->getValue() * 1000) {
                M_SYLAR_LOG_DEBUG(g_logger) << "ping timeout(" << std::to_string(now - recent_pong ) << " > " << std::to_string(g_ws_pong_timeout->getValue()) 
                                            << "), close session, sessionId=" << self->getSessionId();
                co_return false;    // 连接超时，进入关闭流程
            }
            
            co_return true;
        }, 
        [self]()->Task<bool>{
            IOManager::getInstance()->schedule([self]() {
                if(self->getState() != State::OPEN) {
                    M_SYLAR_LOG_DEBUG(g_logger) << "timer" << self->getSessionId();
                    // TimeManager::getInstance()->cancelTimer(self->m_timer_fd);
                    return;     // 连接未处于OPEN状态，无需处理
                }
                // TODO: 实现真正的主动关闭定时器操作
                M_SYLAR_LOG_DEBUG(g_logger) << "ping timeout callback, close session, sessionId=" << self->getSessionId();
                self->m_state = State::CLOSED;
                WsServer::getInstance()->close(self->getSessionId());           // 关闭连接
                // TimeManager::getInstance()->cancelTimer(self->m_timer_fd);
            });
            co_return false;
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
        co_return 1;   // 已经有可用帧，无需继续接收
    }
    uint32_t max_request_size = g_ws_max_request_size->getValue();
    char* buffer = nullptr;     // 输出参数，指向接收数据的起始位置，不要对buffer进行delete操作
    size_t total_length = 0;      // 累计接收字节数

    while(true) {
        int rt = co_await recvMessage(&buffer, -1);
        if(rt == 0) {   // 连接关闭
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
    upDateSessionOnRecv();

    co_return 1;   // 成功接收并解析出至少一帧
}

Task<int> WsSession::co_sendFrame(const Frame& frame) {
    BF_State expected_state = BF_State::READY;
    if(!m_bf_state.compare_exchange_strong(expected_state, BF_State::WAITING)) {
        // 发送缓冲区当前不可用，入队等待
        std::unique_lock<std::shared_mutex> wlock(m_send_mutex);
        m_send_buffer.push_back(frame.make());
        co_return 0;
    } 


    // 发送当前帧
    auto data = frame.make();
    int rt = co_await sendMessage((const char*)data.data(), data.size());
    if(rt == -1) {
        M_SYLAR_LOG_ERROR(g_logger) << "send websocket frame failed, errno:" << errno << " error:" << strerror(errno) << "\nclient:" << getSocket()->toString();
        co_return -1;
    }


    // 当前处于READY状态，可以直接发送
    while(true) {
        std::list<std::vector<uint8_t>> to_send;
        {
            std::unique_lock<std::shared_mutex> wlock(m_send_mutex);
            to_send.swap(m_send_buffer);
            if(to_send.empty()) {
                m_bf_state = BF_State::READY;   // 没有待发送帧，更新状态
                break;
            }   // 释放锁，发送帧
        }
        while(!to_send.empty()) {
            auto& data = to_send.front();
            int rt = co_await sendMessage((const char*)data.data(), data.size());
            if(rt == -1) {
                M_SYLAR_LOG_ERROR(g_logger) << "send websocket frame failed, errno:" << errno << " error:" << strerror(errno) << "\nclient:" << getSocket()->toString();
            }
            to_send.pop_front();
        }
    }
    co_return 0;
}

Task<int> WsSession::co_sendFrame(Frame::ptr frame) {
    co_return co_await co_sendFrame(*frame);
}

Task<int> WsSession::co_close(int code, const std::string& reason) {
    if(m_state == State::CLOSED || m_state == State::CLOSING) {
        co_return 0;   // 已经在关闭中或已关闭，无需重复发送close帧
    }
    if(!getSocket()->isValid()) {// 连接已无效，无需发送close帧,仅更新状态并清理资源
        m_state = State::CLOSED;
        co_return 0;   
    }
    m_state = State::CLOSING;
    Frame f{};
    f.setOpcode(websocket_flags::WS_OP_CLOSE);
    f.setClosePayload(code, reason);
    auto data = f.make();
    M_SYLAR_LOG_DEBUG(g_logger) << "close data: " << std::string(data.begin(), data.end());
    co_await sendMessage((const char*)data.data(), data.size());   // 发送空消息
    // 关闭定时器

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
    std::shared_ptr<WebSocket> ws = std::make_shared<WebSocket>();
    WebSocketFrameType state = ws->parseHandshake((unsigned char*)request_str.c_str(), request_str.size());


    // 2. 构造并发送websocket响应
    if(state != WebSocketFrameType::OPENING_FRAME) {
        M_SYLAR_LOG_WARN(g_logger) << "handshake failed, invalid websocket request, state=" << state;
        co_return -1;
    }
    std::string resp = ws->answerHandshake();
    M_SYLAR_LOG_INFO(g_logger) << "handshake success, send response:\n" << resp;
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
    session->close();
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

