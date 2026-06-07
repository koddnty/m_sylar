#include <protocol/websocket/parser.hpp>
#include <basic/log.h>
#include <basic/until.h>
#include <basic/endian.h>
#include <memory.h>

namespace m_sylar {
namespace websocket {
    
static Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");    

FrameBuffer::FrameBuffer() {
    m_context.parser = &m_parser;
    m_parser.data = &m_context;
    m_context.buffered_frames = &m_buffered_frames;
    websocket_parser_init(&m_parser);
    websocket_parser_settings_init(&m_settings);

    m_settings.on_frame_header = FrameBuffer::on_frame_header;
    m_settings.on_frame_body = FrameBuffer::on_frame_body;
    m_settings.on_frame_end = FrameBuffer::on_frame_end;
}

FrameBuffer::~FrameBuffer() {
    
}

size_t FrameBuffer::consume(unsigned char* in_buffer, int in_length) {
    return websocket_parser_execute(&m_parser, &m_settings, (const char*)in_buffer, in_length);
}


std::shared_ptr<Frame> FrameBuffer::getFrame() {
    if(m_buffered_frames.empty()) {
        return nullptr; // 没有可用帧
    }
    auto frame = m_buffered_frames.front();
    m_buffered_frames.pop_front(); // 从缓冲区移除已取出的帧
    return frame;
}


int FrameBuffer::on_frame_header(websocket_parser* parser) {
    Context* context = (Context*) parser->data;
    context->opcode = (websocket_flags)websocket_parser_get_opcode(parser);
    context->payload_length = parser->length;

    // 重置payload相关状态
    context->payload_binary.clear();
    context->payload_text.clear();
    context->payload_received = 0;

    if(context->opcode == websocket_flags::WS_OP_BINARY) {
        // 二进制帧，准备接收二进制数据
        context->payload_binary.resize(context->payload_length);
    }
    else {
        // 文本帧，准备接收文本数据
        context->payload_text.resize(context->payload_length);
    }  

    return 0; // 返回0表示继续解析
}

int FrameBuffer::on_frame_body(websocket_parser* parser, const char* at, size_t length) {
    Context* context = (Context*) parser->data;
    if(context->payload_received + length > context->payload_length) {
        M_SYLAR_LOG_ERROR(g_logger) << "Received more data than expected: received=" << (context->payload_received + length) << ", expected=" << context->payload_length;
        return -1; // 错误，接收数据超过预期长度
    }

    if(context->opcode == websocket_flags::WS_OP_BINARY) {
        websocket_parser_decode((char*) context->payload_binary.data() + context->payload_received, at, length, parser);
    }
    else {
        websocket_parser_decode(context->payload_text.data() + context->payload_received, at, length, parser);
    } 

    context->payload_received += length;

    return 0;
}

int FrameBuffer::on_frame_end(websocket_parser* parser) {
    Context* context = (Context*) parser->data;
    if(context->payload_received != context->payload_length) {
        M_SYLAR_LOG_ERROR(g_logger) << "Frame end received but payload length mismatch: received=" << context->payload_received << ", expected=" << context->payload_length;
        return -1; // 错误，接收数据长度不匹配
    }

    context->buffered_frames->push_back(std::make_shared<Frame>(std::move(context))); // 将解析完成的帧存储到缓冲区
    return 0; 
}



// 帧
Frame::Frame(FrameBuffer::Context* context) {
    if(!context) {return;}
    m_opcode = context->opcode;
    m_payload_length = context->payload_length;
    m_payload_text = (context->payload_text);
    m_payload_binary = (context->payload_binary);
    // 处理close帧的特殊情况，提取状态码和原因
    if(m_opcode == websocket_flags::WS_OP_CLOSE && m_payload_length >= 2) {
        uint16_t code = 1000; 
        memcpy(&code, context->payload_text.data(), 2);         // 从payload前两字节提取状态码
        m_close_code = NetByteSwapToHostEndian<uint16_t>(code); // 转换为主机字节序，确保正确解析状态码
        m_close_reason = context->payload_text.substr(2);
    }
}

Frame::Frame(FrameBuffer::Context& context) {
    m_opcode = context.opcode;
    m_payload_length = context.payload_length;
    m_payload_text = context.payload_text;
    m_payload_binary = context.payload_binary;
    // 处理close帧的特殊情况，提取状态码和原因
    if(m_opcode == websocket_flags::WS_OP_CLOSE && m_payload_length >= 2) {
        uint16_t code = 1000; 
        memcpy(&code, context.payload_text.data(), 2);         // 从payload前两字节提取状态码
        m_close_code = NetByteSwapToHostEndian<uint16_t>(code); // 转换为主机字节序，确保正确解析状态码
        m_close_reason = context.payload_text.substr(2);
    }
}

Frame::Frame(FrameBuffer::Context&& context) {
    m_opcode = context.opcode;
    // 处理close帧的特殊情况，提取状态码和原因
    if(m_opcode == websocket_flags::WS_OP_CLOSE && m_payload_length >= 2) {
        uint16_t code = 1000; 
        memcpy(&code, context.payload_text.data(), 2);                  // 从payload前两字节提取状态码
        m_close_code = NetByteSwapToHostEndian<uint16_t>(code);         // 转换为主机字节序，确保正确解析状态码
        m_close_reason = context.payload_text.substr(2);
    }
    m_payload_length = context.payload_length;
    m_payload_text = std::move(context.payload_text);
    m_payload_binary = std::move(context.payload_binary);

    context.reset(); // 重置Context状态，清空数据
}

Frame::~Frame() {

}

// 与opcode不一致的payload将被忽略
void Frame::setTextPayload(const std::string& text) {
    m_payload_text = text;
    m_payload_length = text.size();
    return;
}

void Frame::setTextPayload(std::string&& text) {
    m_payload_text = std::move(text);
    m_payload_length = m_payload_text.size();
    return;
}

void Frame::setBinaryPayload(const std::vector<uint8_t>& data) {
    m_payload_binary = data;
    if(m_opcode == websocket_flags::WS_OP_BINARY) {
        m_payload_length = data.size();
    }
    return;
}

void Frame::setBinaryPayload(std::vector<uint8_t>&& data) {
    m_payload_binary = std::move(data);
    if(m_opcode == websocket_flags::WS_OP_BINARY) {
        m_payload_length = m_payload_binary.size();
    }
    return;
}

void Frame::setClosePayload(uint16_t code, const std::string& reason) {
    if(m_opcode != websocket_flags::WS_OP_CLOSE) {
        return;
    }
    m_close_code = code;
    m_close_reason = reason;
    std::string payload;
    payload.resize(2); // 预留状态码位置
    uint16_t bcode = byteSwapToBigEndian(code);
    memcpy(payload.data(), &bcode, 2);
    payload += reason;
    setTextPayload(std::move(payload));
    return;
}

void Frame::setClosePayload(uint16_t code, std::string&& reason) {
    if(m_opcode != websocket_flags::WS_OP_CLOSE) {
        return;
    }
    m_close_code = code;
    m_close_reason = reason;
    std::string payload;
    payload.resize(2); // 预留状态码位置
    uint16_t bcode = byteSwapToBigEndian(code);
    memcpy(payload.data(), &bcode, 2);
    payload += std::move(reason);
    setTextPayload(std::move(payload));
    return;
}

void Frame::setPingPayload(const std::string& msg) {
    setTextPayload(msg);
    return;
}

void Frame::setPingPayload(std::string&& msg) {
    setTextPayload(std::move(msg));
    return;
}

void Frame::setPongPayload(const std::string& msg) {
    setTextPayload(msg);
    return;
}

void Frame::setPongPayload(std::string&& msg) {
    setTextPayload(std::move(msg));
    return;
}



std::vector<uint8_t> Frame::make(bool mask) const {
    std::vector<uint8_t> data;
    make(data, mask);
    return data;
}

int Frame::make(std::vector<uint8_t>& data, bool mask) const {
    data.clear();
    // 第一字节：FIN + RSV1-3 + Opcode
    uint8_t first_byte = 0;
    first_byte |= (m_opcode & WS_OP_MASK); // 设置Opcode
    first_byte |= websocket_flags::WS_FINAL_FRAME; // 设置FIN位
    data.push_back(first_byte);

    // 第二字节及长度扩展：MASK + Payload Length
    uint64_t len = m_payload_length;
    uint8_t byte1 = 0x00;  // 服务端不掩码，MASK = 0
    if(mask) {
        byte1 |= 0x80; // 设置MASK位
    }
    
    if (len < 126) {
        byte1 |= len;
        data.push_back(byte1);
    } else if (len <= 0xFFFF) {
        byte1 |= 126;
        data.push_back(byte1);
        uint16_t len16 = byteSwapToBigEndian((uint16_t)len);
        data.push_back((len16 >> 8) & 0xFF);
        data.push_back(len16 & 0xFF);
    } else {
        byte1 |= 127;
        data.push_back(byte1);
        uint64_t len64 = byteSwapToBigEndian((uint64_t)len);
        for (int i = 56; i >= 0; i -= 8) {
            data.push_back((len64 >> i) & 0xFF);
        }
    }

    // 掩码
    if(mask) {
        uint32_t mask_key = Random<uint32_t>::get(); 
        data.push_back((mask_key >> 24) & 0xFF);
        data.push_back((mask_key >> 16) & 0xFF);
        data.push_back((mask_key >> 8) & 0xFF);
        data.push_back(mask_key & 0xFF);
    }


    // 载荷
    if(m_opcode == websocket_flags::WS_OP_BINARY) {
        data.insert(data.end(), m_payload_binary.begin(), m_payload_binary.end());
    }
    else if(m_opcode == websocket_flags::WS_OP_CLOSE) {
        data.insert(data.end(), m_payload_text.begin(), m_payload_text.end());
    }
    else {
        data.insert(data.end(), m_payload_text.begin(), m_payload_text.end());
    }

    return data.size();
}


    
}
}