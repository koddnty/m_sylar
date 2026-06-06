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


void testBase(IOManager* iom) {
    // auto http_server = http::HttpServer::getInstance(); 
    // m_sylar::Address::ptr addr = m_sylar::Address::LookupAnyIPAddress("0.0.0.0");
    // std::dynamic_pointer_cast<m_sylar::IPv4Address>(addr)->setPort(8803);
    // http_server->bind(addr, 1);
    // http_server->start();

    // auto ws_server = websocket::WsServer::getInstance();
    // ws_server->registerUrl<TestHandler>("/test");
    std::string config_path = "/home/koddnty/user/projects/sylar/m_sylar/m_sylar/conf/basic.json";
    std::cout << "[LoggerManager init] config path: " << config_path << std::endl;
    m_sylar::ConfigManager::LoadJson(config_path, 0);

    http::HttpServer::ptr http_server = std::make_shared<http::HttpServer>(iom);
    m_sylar::Address::ptr addr = m_sylar::Address::LookupAnyIPAddress("0.0.0.0");
    std::dynamic_pointer_cast<m_sylar::IPv4Address>(addr)->setPort(8803);
    http_server->bind(addr, 1);

    websocket::WsServer::ptr ws_server = std::make_shared<websocket::WsServer>(http_server);
    ws_server->registerUrl<TestHandler>("/test");


    http_server->start();

    M_SYLAR_LOG_INFO(g_logger) << "server start";
    sleep(1000);
    http_server->stop();
}



int main(void) {
    m_sylar::IOManager iom {"test websocekt", 1};
    testBase(&iom);

    sleep(100);
    iom.autoStop();
    return 0;
}