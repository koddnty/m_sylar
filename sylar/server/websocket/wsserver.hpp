#pragma once
#include <memory>
#include "basic/log.h"  
#include "basic/socket.h"
#include "basic/config.h"
#include "server/tcp/tcpServer.h"
#include "server/common/session.hpp"
#include "server/http/httpServer.hpp"

#include "protocol/http/parser.hpp"
#include "protocol/http/request.hpp"
#include "protocol/http/response.hpp"
#include "protocol/websocket/WebSocket.h"
#include "protocol/websocket/websocket_parser.h"
#include "protocol/websocket/parser.hpp"
#include "basic/tool.hpp"

namespace m_sylar
{
namespace websocket
{
class WsSession;
static Logger::ptr ghws_logger = M_SYLAR_LOG_NAME("system");

// handler类型
template<typename T>
concept WsHandlerType = requires(std::shared_ptr<WsSession> session, Frame::ptr frame, 
                                  std::string msg, std::vector<uint8_t> data,
                                  int code) {
    // { T::co_Route(session, frame) }   -> std::same_as<Task<int>>;
    { T::co_onOpen(session) }            -> std::same_as<Task<void>>;
    { T::co_onMessage(session, msg) }    -> std::same_as<Task<void>>;
    { T::co_onBinary(session, data) }    -> std::same_as<Task<void>>;
    { T::co_onClose(session, code, msg)} -> std::same_as<Task<void>>;
    { T::co_onPing(session, msg) }      -> std::same_as<Task<void>>;
    { T::co_onPong(session, msg) }      -> std::same_as<Task<void>>;
    { T::co_onError(session, msg) }      -> std::same_as<Task<void>>;
};


/** 
    用户API
*/
class WsHandler {
public:
    using ptr = std::shared_ptr<WsHandler>;
    virtual ~WsHandler() = default;

    template<WsHandlerType T>
    static Task<int> co_Route(std::shared_ptr<WsSession> session, Frame::ptr frame);

    static Task<void> co_onOpen(std::shared_ptr<WsSession> session);                                           // 连接建立
    static Task<void> co_onMessage(std::shared_ptr<WsSession> session, const std::string& msg);               // 文本消息
    static Task<void> co_onBinary(std::shared_ptr<WsSession> session, const std::vector<uint8_t>& data);      // 二进制消息
    static Task<void> co_onClose(std::shared_ptr<WsSession> session, int code, const std::string& reason);    // 连接关闭
    static Task<void> co_onPing(std::shared_ptr<WsSession> session, const std::string& reason);    // ping消息
    static Task<void> co_onPong(std::shared_ptr<WsSession> session, const std::string& reason);    // pong消息
    static Task<void> co_onError(std::shared_ptr<WsSession> session, const std::string& error);               // 连接错误

};



class WsSession : public Session, public std::enable_shared_from_this<WsSession>{
public:
    using ptr = std::shared_ptr<WsSession>;
    WsSession(Socket::ptr socket, size_t sessionId);
    ~WsSession();
    enum class State {
        INIT,           // 连接已建立但未进行定时器配置，不可用状态。等待完成定时器等配置后进入OPEN状态
        OPEN,           // 连接已建立，正常通信中
        CLOSING,        // session关闭中，等待close帧发送完成，底层连接未关闭
        CLOSED          // session关闭，底层连接未关闭，析构后底层连接关闭     
    };

    int init();

    // 所有数据连接，需状态更新
    Task<int> co_recvFrame();         // 消息接受，报文解析, 成功返回1，连接关闭返回0，发生错误返回-1，协议错误等异常情况返回-2
    Task<int> co_sendFrame(const Frame& frame);
    Task<int> co_sendFrame(Frame::ptr frame);
    Task<int> co_close(int code, const std::string& reason);        // 发送close报文

    /** 
        @brief 每次数据通信后更新：更新 最近活跃时间 等等（待扩展
    */
    int upDateSessionOnRecv();          // Frame到来更新函数

    inline void setSessionId(size_t sessionId) { m_sessionId = sessionId; }
    inline void setData(void* data) { m_data = data; }

    inline size_t getSessionId() const { return m_sessionId; }
    inline void* getData() const { return m_data; }
    inline Frame::ptr getFrame() { return m_frame_buffer.getFrame(); }
    inline State getState() const { return m_state; }
    inline uint64_t getRecentActivate() const {return m_recent_activate;}

    // ms
    inline uint64_t getRecentPong() const { return m_recent_pong; }    // 目前ping/pong共用一个时间戳，后续可以根据需要分开
    inline void setRecentPong(uint64_t timestamp) { m_recent_pong = timestamp; }

private:
    size_t m_sessionId;                                      // 会话ID
    uint64_t m_total_received = 0;                              // 累计接收字节数
    uint64_t m_close_timeout = 5000;                    // 关闭连接的超时时间，单位ms
    FrameBuffer m_frame_buffer;                         // 消息帧缓冲区
    std::atomic<State> m_state = State::INIT;                 // 连接状态
    int m_timer_fd = -1;                                    // 定时器fd
    void* m_data = nullptr;
    std::atomic<uint64_t> m_recent_activate{0};
    std::atomic<uint64_t> m_recent_pong{0};             // 最近pong帧的时间戳，单位ms



    enum class BF_State {
        WAITING,        // 不可发送帧，需先入队
        READY           // 可以发送帧
    };

    std::atomic<BF_State> m_bf_state{BF_State::READY};    // 发送缓冲区状态，READY表示可以发送帧，WAITING表示需要先入队帧
    std::shared_mutex m_send_mutex;                       // 发送缓冲区锁，保护
    std::list<std::vector<uint8_t>> m_send_buffer;                 // 待发送帧列表，发送过程中可能会有多个待发送帧
};





/**
    @brief 弱状态websocket服务器，依赖外部http服务器完成握手升级协议，完成后接管连接进行websocket通信流程处理。
        连接管理：sessionId分配器分配sessionId，session维护连接状态和通信流程，wsServer负责调度session进行通信流程处理。
        业务处理：用户继承WsHandler并实现相关函数，wsServer在通信流程处理函数中调用用户函数进行业务处理。
        连接关闭：用户可以在业务处理函数中调用session的co_close函数主动关闭连接，或者在通信流程处理函数中根据用户函数的返回值决定是否关闭连接

        不提供广播等功能，用户需要在业务处理函数中自行维护连接列表实现相关功能
*/
class WsServer : public std::enable_shared_from_this<WsServer>{
public:
    using ptr = std::shared_ptr<WsServer>;
    /**
        @param maxSession*64 服务器最大连接数
     */
    WsServer(http::HttpServer::ptr httpServer, int maxSession = 1024);
    WsServer(http::HttpServer* httpServer, int maxSession = 1024);
    ~WsServer();

    Task<int> handShake(http::HttpSession::ptr http_session);


    /**
        @brief websocket流程处理函数，完成握手后进入该函数进行websocket通信流程处理。
                函数内会持续接收和处理消息，直到连接关闭或发生协议错误等异常情况。
                负责session的打开 处理 关闭全流程。
                不负责底层socket的关闭，底层关闭由session析构时完成。

        @tparam T 处理器类型，必须满足WsHandlerType概念

        @return 是否需要关闭连接，true表示连接已经正常/异常关闭，false表示未关闭连接，需要重新调度后通信。
    */
    template<WsHandlerType T>
    Task<int> handleClient(Socket::ptr client, int sessionId = -1);        // websocket流程处理




    int close(int sessionId);     // 关闭控制，成功返回0，失败返回-1

    /**
        @brief 注册url处理函数，url必须以"/"开头，且不能包含查询参数
            例如："/chat"是合法的url，"/chat?room=1"是非法的url
            模板类型需要继承自WsHandler，且必须实现onOpen、onMessage、onClose等函数
    */
    template<WsHandlerType T>
    void registerUrl(const std::string& url);


    static WsServer* getInstance();

    WsSession::ptr getSession(int sessionId);             // 获取session，成功返回session指针，失败返回nullptr

private:
    WsSession::ptr createSession(Socket::ptr client);     // 创建session，成功返回session指针，失败返回nullptr
    int removeSession(int sessionId);                   // 从session列表移除session，成功返回0，失败返回-1

private:
    std::shared_mutex m_mutex;
    http::HttpServer::ptr m_httpServer;                         // 外部http服务器，完成握手升级协议后传入websocket server
    PackedIDAllocator::ptr m_sessionIdAllocator {nullptr};                 // sessionId分配器
    std::vector<WsSession::ptr> m_sessions;                                  // session列表，存储所有连接的session，索引为sessionId
    std::vector<std::unique_ptr<std::shared_mutex>> m_session_mutexs;                             // session列表锁，保护session列表的读写
    std::atomic<uint32_t> m_sessionCount{0};                             // 当前session数量
};
}
}

#include "wsserver.tpp"

