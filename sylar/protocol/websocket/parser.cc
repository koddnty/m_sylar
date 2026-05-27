#include <protocol/websocket/parser.hpp>
#include <basic/log.h>

namespace m_sylar {
namespace websocket {
    
static Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");    

Frame::Frame() {
    m_context.parser = &m_parser;
    m_parser.data = &m_context;
    websocket_parser_init(&m_parser);
    websocket_parser_settings_init(&m_settings);

    m_settings.on_frame_header = Frame::on_frame_header;
    m_settings.on_frame_body = Frame::on_frame_body;
    m_settings.on_frame_end = Frame::on_frame_end;
}

Frame::~Frame() {
    
}

size_t Frame::consume(unsigned char* in_buffer, int in_length) {
    websocket_parser_execute(&m_parser, &m_settings, (const char*)in_buffer, in_length);
    return in_length; // 这里可以根据实际情况返回已消费的字节数
}



int Frame::on_frame_header(websocket_parser* parser) {
    M_SYLAR_LOG_DEBUG(g_logger) << "Received frame header chunk";
    Context* context = (Context*) parser->data;
    context->opcode = (websocket_flags)websocket_parser_get_opcode(parser);
    context->payload_length = parser->length;

    // 重置payload相关状态
    context->payload_binary.clear();
    context->payload_text.clear();
    context->payload_received = 0;

    if(context->opcode == websocket_flags::WS_OP_TEXT) {
        // 文本帧，准备接收文本数据
        context->payload_text.resize(context->payload_length + 1);
    } else if(context->opcode == websocket_flags::WS_OP_BINARY) {
        // 二进制帧，准备接收二进制数据
        context->payload_binary.resize(context->payload_length);
    }
    else {
        // 其他类型帧，可能需要特殊处理
    }
    return 0; // 返回0表示继续解析
}

int Frame::on_frame_body(websocket_parser* parser, const char* at, size_t length) {
    M_SYLAR_LOG_DEBUG(g_logger) << "Received frame body chunk: length=" << length;
    Context* context = (Context*) parser->data;
    if(context->opcode == websocket_flags::WS_OP_TEXT) {
        websocket_parser_decode(context->payload_text.data() + context->payload_received, at, length, parser);
    } else if(context->opcode == websocket_flags::WS_OP_BINARY) {
        websocket_parser_decode((char*) context->payload_binary.data() + context->payload_received, at, length, parser);
    }
    else {
        M_SYLAR_LOG_DEBUG(g_logger) << "Unsupported opcode: " << (int)context->opcode;
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

int Frame::on_frame_end(websocket_parser* parser) {
    M_SYLAR_LOG_DEBUG(g_logger) << "Frame parsing completed";
    return 0;
}

    
}
}