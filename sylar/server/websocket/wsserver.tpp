#include "wsserver.hpp"

namespace m_sylar
{
namespace websocket
{

static Logger::ptr ghws_logger = M_SYLAR_LOG_NAME("system");

template<WsHandlerType T>
Task<bool, TaskBeginExecuter> WsServer::handleClient(Socket::ptr client) {
    // 外部http服务器已经完成握手升级协议，传入的client是一个websocket连接
    // session 构建
    WsSession::ptr session = createSession(client);
    if(!session) {
        M_SYLAR_LOG_ERROR(ghws_logger) << "handleClient failed, create session failed, error code:" << (int)m_sessionIdAllocator->getErrorCode();
        co_return false;    // 创建session失败，无法处理连接，直接关闭连接
    }
    int sessionId = session->getSessionId();
    if(sessionId < 0) {
        M_SYLAR_LOG_ERROR(ghws_logger) << "handleClient failed, create session failed, error code:" << (int)m_sessionIdAllocator->getErrorCode();
        co_return false;
    }

    int code = 1000;
    int loopCount = 0;
    bool isClosed = false;
    std::string reason {"Normal Closure"};


    // 通信
    do {
        loopCount++;
        // 获取报文
        int rt = co_await session->co_recvFrame();
        // std::cout << "already resume handleClient" << std::endl;
        if(rt == 0) {
            isClosed = true;
            break; /* 连接关闭 */
        }
        else if(rt == -1 && errno != EAGAIN){
            M_SYLAR_LOG_WARN(ghws_logger) << "recv http request failed"
                                        << ", errno:" << errno
                                        << " error:" << strerror(errno)
                                        << "\nclient:" << client->toString();
            isClosed = true;
            break;
        }
        else if(rt == -2){
            isClosed = true;
            break;
        }

        // 处理报文
        bool state = co_await T::co_Route(session, session->getFrame());

        if(state == false) {
            code = 1002;  // 协议错误
            reason = "Protocol Error";
            isClosed = true;
            co_await session->co_close(code, reason);
            break;
        }
    } while (loopCount < 10000 || session->getState() == WsSession::State::CLOSED);     // 限制最大请求数，防止死循环

    // 断开/重新调度
    co_return isClosed;
}


template<WsHandlerType T>
void WsServer::registerUrl(const std::string& url){
        static_assert(std::is_base_of_v<WsHandler, T>, "T must inherit from WsHandler");
        auto self =  shared_from_this();
        m_httpServer->registerUrl(url, [](http::HttpSession::ptr session) -> Task<void> {
            co_await WsServer::getInstance()->handShake(session);                          // 完成握手，建立websocket连接
            bool state = false;
            do {
                state = co_await WsServer::getInstance()->handleClient<T>(session->getSocket());          // 进入websocket流程处理
            } while(!state);
            session->setKeepAlive(false);    // websocket连接不支持长连接，强制设置为短连接
        }, protocol::http::HttpMethod::GET);   // websocket握手协议必须是GET方法
    }
}



}


