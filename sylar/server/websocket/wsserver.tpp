#include "wsserver.hpp"

namespace m_sylar
{
namespace websocket
{

static Logger::ptr ghws_logger = M_SYLAR_LOG_NAME("system");


template<WsHandlerType T>
Task<bool, TaskBeginExecuter> WsServer::handleClient(Socket::ptr client) {
    // 外部http服务器已经完成握手升级协议，传入的client是一个websocket连接
    // 分配sessionId
    int sessionId = m_sessionIdAllocator.alloc();
    if(sessionId < 0) {
        M_SYLAR_LOG_ERROR(ghws_logger) << "handleClient failed, alloc sessionId failed, error code:" << (int)m_sessionIdAllocator.getErrorCode();
        co_return false;
    }
    // session 构建
    WsSession::ptr session = std::make_shared<WsSession>(client, sessionId);
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






    } while (loopCount < 10000);     // 限制最大请求数，防止死循环


    // 断开/重新调度
    co_return isClosed;
}




template<WsHandlerType T>
void WsServer::registerUrl(const std::string& url){
        static_assert(std::is_base_of_v<WsHandler, T>, "T must inherit from WsHandler");
        auto self =  shared_from_this();
        m_httpServer->registerUrl(url, [self](http::HttpSession::ptr session) -> Task<void> {
            co_await self->handShake(session);                          // 完成握手，建立websocket连接
            co_await self->handleClient<T>(session->getSocket());          // 进入websocket流程处理
            session->setKeepAlive(false);    // websocket连接不支持长连接，强制设置为短连接
        }, protocol::http::HttpMethod::GET);   // websocket握手协议必须是GET方法
    }
}
}


