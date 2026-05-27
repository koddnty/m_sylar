#pragma once
#include <protocol/websocket/WebSocket.h>
#include <protocol/websocket/websocket_parser.h>




namespace m_sylar{
namespace websocket {

class Frame {
public:
    Frame();
    ~Frame();

    enum class State {      // Frame解析状态
        INIT,
        HEADER,
        PAYLOAD,
        READY,
        ERROR
    };

    int reset();

    size_t consume(unsigned char* in_buffer, int in_length);

    websocket_flags getType() const { return m_context.opcode; }
    const std::vector<uint8_t>& getBinaryPayload() { return m_context.payload_binary; }
    const std::string& getTextPayload() { return m_context.payload_text; }
    size_t getPayloadLength() const { return m_context.payload_length; }

    struct Context {
        websocket_parser* parser;   // 协议解析器
        websocket_flags opcode = websocket_flags::WS_OP_CONTINUE;
        size_t payload_length {0};
        std::string payload_text {};
        std::vector<uint8_t> payload_binary {};
        size_t payload_received {0};
    };

    static int on_frame_header(websocket_parser* parser);
    static int on_frame_body(websocket_parser* parser, const char* at, size_t length);
    static int on_frame_end(websocket_parser* parser);

private:
    websocket_parser m_parser;
    websocket_parser_settings m_settings;

    State m_state {State::INIT};
    struct Context m_context;
};

}
}