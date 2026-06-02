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


namespace m_sylar
{
namespace websocket
{
class WsSession;
;

/** 
    用户API
*/
class WsHandler {
public:
    using ptr = std::shared_ptr<WsHandler>;
    virtual ~WsHandler() = default;

    virtual Task<void> onOpen(std::shared_ptr<WsSession> session) = 0;                                      // 连接建立
    virtual Task<void> onMessage(std::shared_ptr<WsSession> session, const std::string& msg);               // 文本消息
    virtual Task<void> onBinary(std::shared_ptr<WsSession> session, const std::vector<uint8_t>& data);      // 二进制消息
    virtual Task<void> onClose(std::shared_ptr<WsSession> session, int code, const std::string& reason);    // 连接关闭
    virtual Task<void> onError(std::shared_ptr<WsSession> session, const std::string& error);               // 连接错误

    // virtual Task<void> onPing(const std::string& msg, const std::vector<std::shared_ptr<WsSession>>& sessions);      // 消息广播
};



class WsSession : Session{
public:
    using ptr = std::shared_ptr<WsSession>;
    WsSession(Socket::ptr socket);


    Task<int> co_recvFrame();         // 消息接受，报文解析

    Task<int> co_sendFrame(const std::string& msg);
    Task<int> co_sendFrame(const std::vector<uint8_t>& data);

    Task<int> co_close(int code, const std::string& reason);

private:
    uint64_t m_total_received = 0;                      // 累计接收字节数
    std::shared_ptr<WebSocket> m_ws {nullptr};          // websocket连接状态
    uint64_t m_close_timeout = 5000;                    // 关闭连接的超时时间，单位ms
    FrameBuffer m_frame_buffer;                         // 消息帧缓冲区
};






class WsServer : std::enable_shared_from_this<WsServer>{
public:
    using ptr = std::shared_ptr<WsServer>;
    WsServer();
    ~WsServer();

    Task<int> handShake(http::HttpSession::ptr http_session);
    Task<void, TaskBeginExecuter> handleClient(Socket::ptr client);        // websocket流程处理

    Task<int> close(http::HttpSession::ptr http_session, int code, const std::string& reason);


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
    std::map<std::string, std::pair<std::string, WsHandler::ptr>> m_sessionIdMap;     // sessionId与userId的映射sessionId->userId
    std::map<std::string, std::pair<std::string, WsHandler::ptr>> m_userIdMap;        // userId与sessionId的映射userId->sessionId
};
}
}