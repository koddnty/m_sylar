#include "session.hpp"

namespace m_sylar
{
static Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");
ConfigVar<uint32_t>::ptr g_basic_recv_timeout = ConfigManager::LookUp("servers.basic.timeout.recv", uint32_t(30), 0, "basic server recv timeout");
ConfigVar<uint32_t>::ptr g_basic_send_timeout = ConfigManager::LookUp("servers.basic.timeout.send", uint32_t(30), 0, "basic server send timeout");
ConfigVar<uint32_t>::ptr g_basic_buffer_size = ConfigManager::LookUp("servers.basic.limit.buffer_size", uint32_t(1024), 0, "basic server parser buffer size");


Session::Session(Socket::ptr socket) : m_socket(socket) {
    m_socket->setRecvTimeOut(g_basic_recv_timeout->getValue());
    m_socket->setSendTimeOut(g_basic_send_timeout->getValue());
    int buffer_size = g_basic_buffer_size->getValue();
    m_buffer = new char[buffer_size];
}

Session::~Session() {
    if(m_buffer) {
        delete[] m_buffer;
    }
}


Session::Session(Session&& other) noexcept {
    m_buffer = other.m_buffer;
    m_total_received = other.m_total_received;
    m_buffer_size = other.m_buffer_size;
    m_buffer_begin_pos = other.m_buffer_begin_pos;
    m_buffer_end_pos = other.m_buffer_end_pos;
    m_socket = std::move(other.m_socket);

    other.m_buffer = nullptr;
    other.m_total_received = 0;
    other.m_buffer_size = 0;
    other.m_buffer_begin_pos = 0;
    other.m_buffer_end_pos = 0;
    return ;
}

Session& Session::operator=(Session&& other) {
    if(this == &other) {
        return *this;
    }
    if(m_buffer) {
        delete[] m_buffer;
    }
    m_buffer = other.m_buffer;
    m_total_received = other.m_total_received;
    m_buffer_size = other.m_buffer_size;
    m_buffer_begin_pos = other.m_buffer_begin_pos;
    m_buffer_end_pos = other.m_buffer_end_pos;
    m_socket = std::move(other.m_socket);

    other.m_buffer = nullptr;
    other.m_total_received = 0;
    other.m_buffer_size = 0;
    other.m_buffer_begin_pos = 0;
    other.m_buffer_end_pos = 0;
    return *this;
}




/**
    @brief 接收数据，维护buffer缓冲区
        caution: buffer参数在调用consume后失效，consume会更新buffer状态
    @param buffer 输出参数，指向接收数据的起始位置，不要对buffer进行delete操作
    @param length 接收数据的长度，默认为-1表示接收所有对象中管理的缓冲区数据，但不意味着socket连接缓冲区无数据。
    @return -1 表示较为重要的服务端错误
    @return -2 表示不重要的客户端或其他错误
*/
Task<int> Session::recvMessage(char** buffer, size_t length) {
    // 数据接收
    if(m_buffer_end_pos < m_buffer_size) {      // 缓冲区可接收
        int recv_len = co_await m_socket->recv(m_buffer + m_buffer_end_pos, m_buffer_size - m_buffer_end_pos, 0);
        if(recv_len == 0)
        {   // 连接关闭
            co_return 0;
        }
        else if(recv_len == -1 || recv_len == -2)
        {   // 错误
            switch(errno) {
                case EBADF:        // 9 - 无效文件描述符
                case EFAULT:       // 14 - 无效内存地址
                case ENOMEM:       // 12 - 内存不足
                case EMFILE:       // 24 - 打开文件过多
                case ENFILE:       // 23 - 系统文件表满
                    M_SYLAR_LOG_ERROR(g_logger) << "recv critical error: " << strerror(errno);
                    co_return -1;
                    
                default:
                    // 其他所有错误（ ECONNRESET 等）都静默处理
                    co_return -2;
            }
        }
        m_total_received += recv_len;
        m_buffer_end_pos += recv_len;
        M_SYLAR_ASSERT2(m_buffer_end_pos <= m_buffer_size, "m_buffer_end_pos out of range");
    }

    // 返回数据
    M_SYLAR_LOG_DEBUG(g_logger) << "recv data, length=" << (m_buffer_end_pos)  << "   " <<  (m_buffer_begin_pos) << " total received=" << m_total_received;
    int rt_length = m_buffer_end_pos - m_buffer_begin_pos;
    rt_length = (rt_length < length) ? rt_length : length;
    *buffer = m_buffer + m_buffer_begin_pos;
    

    co_return rt_length;
}

/**
    @brief 消费数据，更新buffer状态
    @param length 消费数据的长度
    @return 实际消费的长度，可能小于length，调用者需要根据返回值判断buffer状态
*/
size_t Session::consume(size_t length) {
    // 缓冲区状态更新
    size_t consumed  = length;
    m_buffer_begin_pos += length;
    if(m_buffer_begin_pos >= m_buffer_end_pos) {     // 已经处理完所有数据，重置缓冲区
        consumed -= (m_buffer_begin_pos - m_buffer_end_pos);
        m_buffer_begin_pos = 0;
        m_buffer_end_pos = 0;
    }
    return consumed;
}

Task<int> Session::sendMessage(const char* buffer, size_t length) {
    co_return co_await m_socket->send(buffer, length, 0);
}

};