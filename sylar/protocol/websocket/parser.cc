#include <protocol/websocket/parser.hpp>
#include <basic/log.h>

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
    else if(context->opcode == websocket_flags::WS_OP_TEXT) {
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
    // if(websocket_parser_has_mask(parser)) {
    //     // 处理掩码
    //     for(size_t i = 0; i < length; ++i) {
    //         uint8_t masked_byte = at[i] ^ parser->mask[(context->payload_received + i) % 4];
    //         if(context->opcode == websocket_flags::WS_OP_TEXT) {
    //             context->payload_text.append(at, length);
    //         } else if(context->opcode == websocket_flags::WS_OP_BINARY) {
    //             context->payload_binary.push_back(masked_byte);
    //         }
    //         else {
    //             M_SYLAR_LOG_DEBUG(g_logger) << "Unsupported opcode: " << (int)context->opcode;
    //         }
    //     }
    // } else {
    //     // 无掩码，直接添加
    //     if(context->opcode == websocket_flags::WS_OP_TEXT) {
    //         context->payload_text.append(at, length);
    //     } else if(context->opcode == websocket_flags::WS_OP_BINARY) {
    //         context->payload_binary.insert(context->payload_binary.end(), at, at + length);
    //     }
    //     else {
    //         M_SYLAR_LOG_DEBUG(g_logger) << "Unsupported opcode: " << (int)context->opcode;
    //     }
    // }

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
}

Frame::Frame(FrameBuffer::Context& context) {
    m_opcode = context.opcode;
    m_payload_length = context.payload_length;
    m_payload_text = context.payload_text;
    m_payload_binary = context.payload_binary;
}

Frame::Frame(FrameBuffer::Context&& context) {
    m_opcode = context.opcode;
    m_payload_length = context.payload_length;
    m_payload_text = std::move(context.payload_text);
    m_payload_binary = std::move(context.payload_binary);

    context.reset(); // 重置Context状态，清空数据
}

Frame::~Frame() {

}


    
}
}