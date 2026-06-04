#pragma once
#include <protocol/websocket/WebSocket.h>
#include <protocol/websocket/websocket_parser.h>
#include <memory>
#include <list>
#include <basic/singleton.h>
#include <random>

namespace m_sylar{
namespace websocket {

template<typename T>
class Random {
public:
    static T get() {
        return getGenerator()();
    }

    static T get(T min, T max) {
        std::uniform_int_distribution<T> dist(min, max);
        return getGenerator()(dist);
    }

private: 
    static std::mt19937& getGenerator() {
        thread_local std::random_device rd;
        thread_local std::mt19937 gen(rd());
        return gen;
    }
};

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


    /**
        @brief websocket_parser回调函数，解析过程中会被调用,生成的frame全部无掩码
    */
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
    Frame() = default;
    ~Frame();

    websocket_flags getType() const { return m_opcode; }
    const std::vector<uint8_t>& getBinaryPayload() { return m_payload_binary; }
    const std::string& getTextPayload() { return m_payload_text; }
    size_t getPayloadLength() const { return m_payload_length; }

    void clear();

    void setOpcode(websocket_flags opcode) { m_opcode = opcode; }
    /**
        @brief 与opcode不一致的payload将被忽略
            除了二进制帧，其他帧共享buffer,因此设置closePayload时会覆盖其他类型payload
    */ 
    void setTextPayload(const std::string& text);
    void setTextPayload(std::string&& text);
    void setBinaryPayload(const std::vector<uint8_t>& data);
    void setBinaryPayload(std::vector<uint8_t>&& data);
    void setClosePayload(uint16_t code, const std::string& reason);
    void setClosePayload(uint16_t code, std::string&& reason);
    void setPingPayload(const std::string& msg);
    void setPingPayload(std::string&& msg);
    void setPongPayload(const std::string& msg);
    void setPongPayload(std::string&& msg);

    inline const std::string& getPayload() const { return m_close_reason; }
    inline const std::vector<uint8_t>& getBinaryPayload() const { return m_payload_binary; }

    // 若frame不是close帧，则返回-1，reason为空字符串.
    inline int getCloseCode() const { return m_close_code; }
    inline const std::string& getCloseReason() const { return m_close_reason; }


    std::vector<uint8_t> make(bool mask = false) const;   // 将帧内容编码为二进制数据，准备发送
    int make(std::vector<uint8_t>& data, bool mask = false) const;   // 清空容器数据，将帧内容编码为二进制数据，准备发送

private:
    websocket_flags m_opcode = websocket_flags::WS_OP_CONTINUE;
    size_t m_payload_length {0};
    std::string m_payload_text {};
    std::vector<uint8_t> m_payload_binary {};

    uint16_t m_close_code {1000};
    std::string m_close_reason {};
};

}
}