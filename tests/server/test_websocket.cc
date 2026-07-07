#include <server/websocket/wsserver.hpp>
#include <server/http/httpServer.hpp>
/**
    存在问题,ping,pong报文排队导致连接超时
*/
using namespace m_sylar;

static Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");
#include <signal.h>

void ignore_sigpipe() {
    signal(SIGPIPE, SIG_IGN);
    // 或者使用sigaction
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGPIPE, &sa, nullptr);
}


class TestHandler : public websocket::WsHandler {
public:
    static Task<void> co_onOpen(std::shared_ptr<websocket::WsSession> session){
        M_SYLAR_LOG_INFO(g_logger) << "successfully open websocket connection, sessionId: \n-----------------oh yeah----------------- " << session->getSessionId();
        co_return;
    }                                       // 连接建立


    static Task<void> co_onMessage(std::shared_ptr<websocket::WsSession> session, const std::string& msg) {
        M_SYLAR_LOG_INFO(g_logger) << "received websocket message, sessionId:" << session->getSessionId() << ", msg:" << msg;
        std::string reply = "Echo: " + msg;
        websocket::Frame f;
        f.setOpcode(websocket_flags::WS_OP_TEXT);
        f.setTextPayload(reply);

        auto data = f.make();   // 构造帧数据
        auto first = data[0];
        uint8_t first_byte = data[0];
        bool FIN  = (first_byte >> 7) & 1;
        bool RSV1 = (first_byte >> 6) & 1;
        bool RSV2 = (first_byte >> 5) & 1;
        bool RSV3 = (first_byte >> 4) & 1;
        int  opcode = first_byte & 0x0F;

        M_SYLAR_LOG_INFO(g_logger) << "FIN=" << FIN << " RSV1=" << RSV1 << " RSV2=" << RSV2 << " RSV3=" << RSV3 << " opcode=" << opcode;
        M_SYLAR_LOG_INFO(g_logger) << "reply frame, sessionId=" << session->getSessionId() << ", opcode=" << opcode << ", payload length=" << f.getPayloadLength();
        for(int i = 0; i < 1000; i++) {
            co_await co_sleep(5);   // 每30ms秒发送一次消息
            f.setTextPayload(std::to_string(i));
            co_await session->co_sendFrame(f);   // 回复消息
        }
        co_return;
    }
    

};


void testBase(IOManager* iom) {
    // auto http_server = http::HttpServer::getInstance(); 
    // m_sylar::Address::ptr addr = m_sylar::Address::LookupAnyIPAddress("0.0.0.0");
    // std::dynamic_pointer_cast<m_sylar::IPv4Address>(addr)->setPort(8803);
    // http_server->bind(addr, 1);
    // http_server->start();

    // auto ws_server = websocket::WsServer::getInstance();
    // ws_server->registerUrl<TestHandler>("/test");
    ignore_sigpipe();  // 忽略SIGPIPE
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
    sleep(600);
    http_server->stop();
}



int main(void) {

    std::string config_path = "/home/koddnty/user/projects/sylar/m_sylar/m_sylar/conf/basic.json";
    std::cout << "[LoggerManager init] config path: " << config_path << std::endl;
    m_sylar::ConfigManager::LoadJson(config_path, 0);
    
    m_sylar::IOManager iom {"test websocekt", 4};
    testBase(&iom);

    sleep(100);
    iom.autoStop();
    return 0;
}