#pragma once
#include "basic/log.h"
#include "basic/socket.h"



namespace m_sylar{


/** 
    @brief 会话管理类
        维护会话相关数据信息，如流式的buffer，会话状态等
*/
class Session {
public:
    using ptr = std::shared_ptr<Session>;
    Session(Socket::ptr socket);
    ~Session();

    Task<int> recvMessage(char** buffer, size_t length = -1);               // 接受一些数据，返回接收字节数,参数buffer在第二次同调用时失效
    Task<int> sendMessage(const char* buffer, size_t length);              // 发送数据，返回发送字节数
    size_t consume(size_t length);

    void setRecvTimeOut(uint64_t time) { return m_socket->setRecvTimeOut(time); }
    void setSendTimeOut(uint64_t time) { return m_socket->setSendTimeOut(time); }
    void setBufferSize(size_t size) { m_buffer_size = size; }

    int64_t getRecvTimeOut() const { return m_socket->getRecvTimeOut(); }
    int64_t getSendTimeOut() const { return m_socket->getSendTimeOut(); }
    inline int64_t getBufferSize() const { return m_buffer_size; }

    const Socket::ptr getSocket() const { return m_socket; }


private:
    char* m_buffer;               // 流式buffer
    uint64_t m_total_received = 0;           // 累计接收字节数
    size_t m_buffer_size;          // buffer大小
    size_t m_buffer_begin_pos = 0;       // buffer中未处理数据的起始位置
    size_t m_buffer_end_pos = 0;         // buffer中未处理数据的结束位置
    Socket::ptr m_socket;         // 会话对应的socket
};
}





