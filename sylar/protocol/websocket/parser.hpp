#pragma once
#include <protocol/websocket/WebSocket.h>
#include <protocol/websocket/websocket_parser.h>
#include <memory>
#include <list>


namespace m_sylar{
namespace websocket {

class Frame;

class FrameBuffer {
public:
    using ptr = std::shared_ptr<FrameBuffer>;
    friend class Frame;
    FrameBuffer();
    ~FrameBuffer();

    // 消息解析，返回已解析的字节数。报文通过getFrame获取
    size_t consume(unsigned char* in_buffer, int in_length);

    size_t getFrameCount() const { return m_buffered_frames.size(); }
    std::shared_ptr<Frame> getFrame();      // 取出一个完整帧，buffer将删除该帧数据


    static int on_frame_header(websocket_parser* parser);
    static int on_frame_body(websocket_parser* parser, const char* at, size_t length);
    static int on_frame_end(websocket_parser* parser);


protected:
    struct Context {
        void reset() {
            opcode = websocket_flags::WS_OP_CONTINUE;
            payload_length = 0;
            payload_text.clear();
            payload_binary.clear();
            payload_received = 0;
        }
        websocket_parser* parser;   // 协议解析器
        std::list<std::shared_ptr<Frame>>* buffered_frames; // 指向FrameBuffer的帧缓冲区，用于存储解析完成的帧
        websocket_flags opcode = websocket_flags::WS_OP_CONTINUE;
        size_t payload_length {0};
        std::string payload_text {};
        std::vector<uint8_t> payload_binary {};
        size_t payload_received {0};
    };

private:
    websocket_parser m_parser;
    websocket_parser_settings m_settings;

    struct Context m_context;
    std::list<std::shared_ptr<Frame>> m_buffered_frames; // 用于存储解析完成的帧
};


class Frame{
public:
    using ptr = std::shared_ptr<Frame>;

    Frame(FrameBuffer::Context* context);
    Frame(FrameBuffer::Context& context);
    Frame(FrameBuffer::Context&& context);
    ~Frame();

    websocket_flags getType() const { return m_opcode; }
    const std::vector<uint8_t>& getBinaryPayload() { return m_payload_binary; }
    const std::string& getTextPayload() { return m_payload_text; }
    size_t getPayloadLength() const { return m_payload_length; }

private:
    websocket_flags m_opcode = websocket_flags::WS_OP_CONTINUE;
    size_t m_payload_length {0};
    std::string m_payload_text {};
    std::vector<uint8_t> m_payload_binary {};
};

}
}