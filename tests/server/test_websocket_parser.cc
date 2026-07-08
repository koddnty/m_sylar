#include <protocol/websocket/parser.hpp>
#include <iostream>
#include <basic/config.h>
#include <basic/log.h>


static m_sylar::Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");
void loadConfig() {
    std::string path = "/home/koddnty/user/projects/sylar/m_sylar/m_sylar/conf/basic.json";
    m_sylar::ConfigManager::LoadJson(path, 0);
}


std::vector<uint8_t> makeFrame(const std::string& payload) {
    // Masking Key (4 字节，可以随便选)
    uint8_t mask_key[4] = {0x12, 0x34, 0x56, 0x78};
    
    // 对 payload 进行掩码
    std::vector<uint8_t> masked_payload(payload.size());
    for (size_t i = 0; i < payload.size(); ++i) {
        masked_payload[i] = payload[i] ^ mask_key[i % 4];
    }
    
    // 构造帧：MASK=1
    std::vector<uint8_t> frame;
    frame.push_back(0x81);                    // FIN=1, opcode=0x1
    frame.push_back(0x80 | payload.size());                // MASK=1, payload len=5 (0x85)
    frame.insert(frame.end(), mask_key, mask_key + 4);  // Masking Key
    frame.insert(frame.end(), masked_payload.begin(), masked_payload.end());  // 掩码后的数据
    return frame;
}

void testSingleParser() {
    M_SYLAR_LOG_DEBUG(g_logger) << "Starting single frame test";
    // 原始 payload


    auto frame = makeFrame("std::string &payload");

    M_SYLAR_LOG_INFO(g_logger) << "frame size: " << frame.size();
    
    m_sylar::websocket::FrameBuffer parser;
    int consumed = 0;
    while(consumed < frame.size()) {
        consumed += parser.consume(frame.data() + consumed, 8);
        std::cout << "Consumed " << consumed << " bytes " << std::endl;
    }

    auto f = parser.getFrame();
    std::string text = f->getTextPayload();
    auto binary = f->getBinaryPayload();
    std::cout << "consumed: " << consumed << std::endl;
    std::cout << "text: " << text << std::endl;
    std::cout << "binary: ";
    for(auto b : binary) {
        std::cout << std::hex << static_cast<int>(b) << " ";
    }
    std::cout << std::dec << std::endl;

    return;
}

void testMultipleFrames() {
    M_SYLAR_LOG_DEBUG(g_logger) << "Starting multiple frames test";
    // 这里可以构造多个帧并测试解析器的连续解析能力

    auto frame1 = makeFrame("Hello ");
    auto& combined = frame1;
    for(int i = 0; i < 12; i++) {
        auto frame2 = makeFrame("World!" + std::to_string(i));
        combined.insert(combined.end(), frame2.begin(), frame2.end());
    }

    M_SYLAR_LOG_INFO(g_logger) << "combined size: " << combined.size();

    m_sylar::websocket::FrameBuffer frame;

    int consumed = 0;
    while(consumed < combined.size()) {
        consumed += frame.consume(combined.data() + consumed,(24 <  combined.size() - consumed) ? 24 : combined.size() - consumed   );
    }
    int total_frames = frame.getFrameCount();
    M_SYLAR_LOG_INFO(g_logger) << "Total frames parsed: " << total_frames;
    for(size_t i = 0; i < total_frames; ++i) {
        m_sylar::websocket::Frame::ptr f = frame.getFrame();
        std::cout << "Frame " << i << ": type=" << (int)f->getType() << ", payload_length=" << f->getPayloadLength() << ", text=" << f->getTextPayload() << std::endl;
    }
    return;
}


using namespace m_sylar::websocket;
void testMake(bool mask) {
    Frame f{};
    f.setOpcode(websocket_flags::WS_OP_TEXT);
    f.setTextPayload("Hello WebSocket");
    std::vector<uint8_t> data = f.make(mask);

    std::cout << "Generated frame size: " << data.size() << std::endl;
    std::cout << "Generated frame data: \n================\n" ;
    for (auto b : data) printf("%02x ", b);
    std::cout << "\n================\n" ;
    
    // parser
    FrameBuffer parser;
    int consumed = 0;
    while(consumed < data.size()) {
        consumed += parser.consume(data.data() + consumed, 8);
    }

    auto frame = parser.getFrame();
    std::cout << "Parsed frame: type=" << (int)frame->getType() 
                << "\n payload_length=" << frame->getPayloadLength() 
                << ", payload=" << frame->getTextPayload() << std::endl;

    return ;
}


int main(void){
    loadConfig();
    // testSingleParser();

    // testMultipleFrames();

    testMake(false);

    return 0;
}