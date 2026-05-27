#include <protocol/websocket/parser.hpp>
#include <iostream>
#include <basic/config.h>
#include <basic/log.h>


static m_sylar::Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");
void loadConfig() {
    std::string path = "/home/koddnty/user/projects/sylar/m_sylar/m_sylar/conf/basic.json";
    m_sylar::ConfigManager::LoadJson(path, 0);
}

void testParser() {
    M_SYLAR_LOG_DEBUG(g_logger) << "Starting WebSocket parser test";
    // 原始 payload
    std::string original = "Hello";
    
    // Masking Key (4 字节，可以随便选)
    uint8_t mask_key[4] = {0x12, 0x34, 0x56, 0x78};
    
    // 对 payload 进行掩码
    std::vector<uint8_t> masked_payload(5);
    for (size_t i = 0; i < 5; ++i) {
        masked_payload[i] = original[i] ^ mask_key[i % 4];
    }
    
    // 构造帧：MASK=1
    std::vector<uint8_t> frame;
    frame.push_back(0x81);                    // FIN=1, opcode=0x1
    frame.push_back(0x80 | 5);                // MASK=1, payload len=5 (0x85)
    frame.insert(frame.end(), mask_key, mask_key + 4);  // Masking Key
    frame.insert(frame.end(), masked_payload.begin(), masked_payload.end());  // 掩码后的数据
    
    m_sylar::websocket::Frame parser;
    int consumed = parser.consume(frame.data(), frame.size());
    std::vector<uint8_t> binary = parser.getBinaryPayload();
    std::string text = parser.getTextPayload();
    std::cout << "consumed: " << consumed << std::endl;
    std::cout << "text: " << text << std::endl;
    std::cout << "binary: ";
    for(auto b : binary) {
        std::cout << std::hex << static_cast<int>(b) << " ";
    }
    std::cout << std::dec << std::endl;



    return;
}

int main(void){
    loadConfig();
    testParser();

    return 0;
}