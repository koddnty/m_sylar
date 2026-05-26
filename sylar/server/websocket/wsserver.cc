#include "server/websocket/wsserver.hpp"
#include "protocol/websocket/WebSocket.h"
#include <random>

namespace m_sylar {
namespace websocket {

Task<int> WsServer::handShake(http::HttpSession::ptr http_session) {
    // 完成握手，建立websocket连接
    // 请求判断
    std::shared_ptr<protocol::http::parser::request> request = http_session->getRequest();

    std::string request_str = request->raw();
    std::shared_ptr<WebSocket> ws {};
    WebSocketFrameType state = ws->parseHandshake((unsigned char*)request_str.c_str(), request_str.size());


    // 2. 构造websocket响应
    if(state == WebSocketFrameType::ERROR_FRAME) {
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

    // 通信
    // 断开
    co_return;
}



}
}