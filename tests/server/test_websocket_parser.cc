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



void testBadFrames() {
    M_SYLAR_LOG_DEBUG(g_logger) << "Starting bad frames test";
    // 这里可以构造一些格式错误的帧，测试解析器的错误处理能力

    std::vector<uint8_t> bad_frame;
    std::string message = "This is a bad frame";
    bad_frame.insert(bad_frame.end(), message.begin(), message.end());

    m_sylar::websocket::FrameBuffer fb;
    int consumed = 0;
    while(consumed < bad_frame.size()) {
        consumed += fb.consume(bad_frame.data() + consumed, (8 <  bad_frame.size() - consumed) ? 24 : bad_frame.size() - consumed   );
    }

    auto frame = fb.getFrame();
    if(!frame) {
        std::cout << "no data";
    } else {
        std::cout << "Frame type=" << (int)frame->getType() << ", payload_length=" << frame->getPayloadLength() << ", text=" << frame->getTextPayload() << std::endl;
        std::cout << "No frame parsed from bad data, as expected." << std::endl;
    }

    return;
}


void testBinaryFrames() {
    M_SYLAR_LOG_DEBUG(g_logger) << "Starting binary frames test";
    // 这里可以构造一些二进制帧，测试解析器对二进制数据的处理能力

    std::vector<uint8_t> binary_payload = {0x01, 0x02, 0x03, 0x04, 0x05};
    auto frame_data = makeFrame(std::string(binary_payload.begin(), binary_payload.end()));

    m_sylar::websocket::FrameBuffer fb;
    int consumed = 0;
    while(consumed < frame_data.size()) {
        consumed += fb.consume(frame_data.data() + consumed, (8 <  frame_data.size() - consumed) ? 24 : frame_data.size() - consumed   );
    }

    auto frame = fb.getFrame();
    if(frame) {
        std::cout << "Frame type=" << (int)frame->getType() << ", payload_length=" << frame->getPayloadLength() << ", binary data: ";
        for(auto b : frame->getBinaryPayload()) {
            std::cout << std::hex << static_cast<int>(b) << " ";
        }
        std::cout << std::dec << std::endl;
    } else {
        std::cout << "Failed to parse binary frame." << std::endl;
    }
}

int main(void){
    loadConfig();
    // testSingleParser();

    //testMultipleFrames();


    // testBadFrames();

    testBinaryFrames();
    return 0;
}