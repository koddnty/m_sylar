#include <server/websocket/wsserver.hpp>
#include <server/http/httpServer.hpp>

using namespace m_sylar;

static Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");


class TestHandler : public websocket::WsHandler {
public:
    static Task<void> co_onOpen(std::shared_ptr<websocket::WsSession> session){
        M_SYLAR_LOG_INFO(g_logger) << "successfully open websocket connection, sessionId:" << session->getSessionId();
        co_return;
    }                                       // 连接建立

};


void testBase() {
    auto ws_server = websocket::WsServer::getInstance();
    ws_server->registerUrl<TestHandler>("/test");
}



int main(void) {
    m_sylar::IOManager::ptr iom = std::make_shared<IOManager> ("test websocekt", 1);

    iom->schedule(testBase);

    return 0;
}