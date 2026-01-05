#pragma once
#include "address.h"
#include <cstddef>
#include <cstdint>
#include <getopt.h>
#include <memory.h>
#include <memory>
#include <ostream>
#include <sys/socket.h>
#include <sys/uio.h>



namespace m_sylar
{

class Socket : public std::enable_shared_from_this<Socket>
{
public:
    using ptr = std::shared_ptr<Socket>;
    using weak_ptr = std::shared_ptr<Socket>;

    Socket(int family, int type, int protocol);
    ~Socket();

    static ptr CreateTCP(m_sylar::Address::ptr address);
    static ptr CreateUDP(m_sylar::Address::ptr address);

    static ptr CreateTCPSocket();
    static ptr CreateUDPSocket();

    static ptr CreateTCPSocket6();
    static ptr CreateUDPSocket6();

    
    static ptr CreateUnixTCPSocket();
    static ptr CreateUnixUDPSocket();
    
    int64_t getSendTimeOut();           // usec
    void setSendTimeOut(int64_t time);  // usec  

    int64_t getRecvTimeOut();           // usec
    void setRecvTimeOut(int64_t time);  // usec

    bool getOption(int level, int option, void* result, socklen_t* len);        // get socket option, success on true
    template<typename T>
    bool getOption(int level, int option, T* result)
    {
        size_t len = sizeof(T);
        return getsockopt(level, option, result, &len);
    }

    bool setOption(int level, int option, const void* value, socklen_t len);    // set socket option, success on true
    template<typename T>
    bool setOption(int level, int option, const T* value)
    {
        size_t len {sizeof(T)};
        return setsockopt(m_sock_fd, level, option, value, len);
    }


    bool init(int sock_fd);         // create a Socket from socketFd
    bool bind(const Address::ptr addr);
    bool listen(int backlog = SOMAXCONN);
    Socket::ptr accept();
    bool connect(const Address::ptr addr, uint64_t timeOut = -1);       // client
    bool close();

    int send(const void* buffer, size_t length, int flags = 0);
    int send(const iovec* buffers, size_t length, int flags = 0);
    int sendTo(Address::ptr to, const void* buffer, size_t length, int flags = 0);
    int sendTo(Address::ptr to, const iovec* buffers, size_t length, int flags = 0);

    int recv(const void* buffer, size_t length, int flags = 0);
    int recv(const iovec* buffers, size_t length, int flags = 0);
    int recvFrom(Address::ptr from, const void* buffers, size_t length, int flags = 0);
    int recvFrom(Address::ptr from, const iovec* buffers, size_t length, int flags = 0);

    Address::ptr getRemoteAddress();
    Address::ptr getLocalAddress();

    int getFamily() const {return m_family;}
    int getType() const {return m_type;}
    int Protocol() const {return m_protocol;}

    std::ostream& dump(std::ostream& os) const;
    int getSocket() const {return m_sock_fd;}

    bool cancelRead();
    bool cancelWrite();
    bool cancelAll();

    bool isValid();

private:
    void initSock();
    void newSock();
private:
    int m_sock_fd;              // socket fd
    int m_family;               // socket family (AF_INET...)
    int m_protocol;             // protocol (OSTREAM)
    int m_type;                 // type
    bool m_isConnected;

    Address::ptr m_local_address;
    Address::ptr m_remote_address;
};

}