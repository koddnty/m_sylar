#pragma once
#include <memory>
#include "basic/log.h"  
#include "basic/socket.h"
#include "basic/config.h"
#include "server/tcp/tcpServer.h"
#include "server/common/session.hpp"
#include "server/http/httpServer.h"

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

// handler类型
template<typename T>
concept WsHandlerType = requires(std::shared_ptr<WsSession> session, Frame::ptr frame, 
                                  std::string msg, std::vector<uint8_t> data,
                                  int code) {
    { T::co_Route(session, frame) }   -> std::same_as<Task<bool>>;
    { T::onOpen(session) }            -> std::same_as<Task<void>>;
    { T::onMessage(session, msg) }    -> std::same_as<Task<void>>;
    { T::onBinary(session, data) }    -> std::same_as<Task<void>>;
    { T::onClose(session, code, msg)} -> std::same_as<Task<void>>;
    { T::onError(session, msg) }      -> std::same_as<Task<void>>;
};


/** 
    用户API
*/
class WsHandler {
public:
    using ptr = std::shared_ptr<WsHandler>;
    virtual ~WsHandler() = default;

    static Task<int> co_Route(std::shared_ptr<WsSession> session, Frame::ptr frame);

    static Task<void> co_onOpen(std::shared_ptr<WsSession> session);                                           // 连接建立
    static Task<void> co_onMessage(std::shared_ptr<WsSession> session, const std::string& msg);               // 文本消息
    static Task<void> co_onBinary(std::shared_ptr<WsSession> session, const std::vector<uint8_t>& data);      // 二进制消息
    static Task<void> co_onClose(std::shared_ptr<WsSession> session, int code, const std::string& reason);    // 连接关闭
    static Task<void> co_onPing(std::shared_ptr<WsSession> session, const std::string& reason);    // ping消息
    static Task<void> co_onPong(std::shared_ptr<WsSession> session, const std::string& reason);    // pong消息
    static Task<void> co_onError(std::shared_ptr<WsSession> session, const std::string& error);               // 连接错误

};



class WsSession : Session{
public:
    using ptr = std::shared_ptr<WsSession>;
    WsSession(Socket::ptr socket, size_t sessionId);
    enum class State {
        INIT,
        OPEN,
        CLOSING,
        CLOSED
    };


    Task<int> co_recvFrame();         // 消息接受，报文解析

    Task<int> co_sendFrame(const Frame& frame);
    Task<int> co_sendFrame(Frame::ptr frame);

    Task<int> co_close(int code, const std::string& reason);

    size_t getSessionId() const { return m_sessionId; }
    void setSessionId(size_t sessionId) { m_sessionId = sessionId; }

    inline void setData(void* data) { m_data = data; }
    inline void* getData() const { return m_data; }

    Frame::ptr getFrame() { return m_frame_buffer.getFrame(); }

private:
    size_t m_sessionId;                             // 会话ID
    uint64_t m_total_received = 0;                      // 累计接收字节数
    uint64_t m_close_timeout = 5000;                    // 关闭连接的超时时间，单位ms
    FrameBuffer m_frame_buffer;                         // 消息帧缓冲区
    State m_state = State::INIT;                 // 连接状态

    void* m_data = nullptr;
};






class WsServer : public std::enable_shared_from_this<WsServer>{
public:
    using ptr = std::shared_ptr<WsServer>;
    /**
        @param maxSession*64 服务器最大连接数
     */
    WsServer(http::HttpServer::ptr httpServer, int maxSession = 64);
    ~WsServer();

    Task<int> handShake(http::HttpSession::ptr http_session);

    template<WsHandlerType T>
    Task<bool, TaskBeginExecuter> handleClient(Socket::ptr client);        // websocket流程处理

    Task<int> close(http::HttpSession::ptr http_session, int code, const std::string& reason);

    /**
        @brief 注册url处理函数，url必须以"/"开头，且不能包含查询参数
            例如："/chat"是合法的url，"/chat?room=1"是非法的url
            模板类型需要继承自WsHandler，且必须实现onOpen、onMessage、onClose等函数
    */
    template<WsHandlerType T>
    void registerUrl(const std::string& url);


    // 消息广播函数
    /**
        @brief 消息广播函数
            1. 广播文本消息
            2. 广播二进制消息
            3. 广播消息时可以指定sessionId或者userId，或者直接指定session对象
        
        @param failes 可选参数，广播失败的session会在failes中标记为true
        
        @return 失败广播的消息数量
    */
    Task<int> broadcast(const std::string& msg, std::vector<WsSession::ptr> sessions, std::shared_ptr<std::vector<bool>> failes = nullptr);      // 广播文本消息
    Task<int> broadcastBySessionId(const std::string& msg, std::vector<std::string> sessions, std::shared_ptr<std::vector<bool>> failes = nullptr);      // 广播文本消息
    Task<int> broadcastByUserId(const std::string& msg, std::vector<std::string> sessions, std::shared_ptr<std::vector<bool>> failes = nullptr);      // 广播文本消息


private:
    std::shared_mutex m_mutex;
    http::HttpServer::ptr m_httpServer;             // 外部http服务器，完成握手升级协议后传入websocket server
    PackedIDAllocator m_sessionIdAllocator{64};     // sessionId分配器
    std::map<std::string, std::pair<std::string, WsHandler::ptr>> m_sessionIdMap;     // sessionId与userId的映射sessionId->userId
    std::map<std::string, std::pair<std::string, WsHandler::ptr>> m_userIdMap;        // userId与sessionId的映射userId->sessionId
};
}
}

