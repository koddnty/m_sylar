#pragma once
#include <cstdint>
#include <iostream>
#include <memory>
#include <netinet/in.h>
#include <ostream>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <sys/un.h>
#include <unistd.h>
#include "log.h"


namespace m_sylar
{

class Address
{
public:
    using ptr = std::shared_ptr<Address>;
    Address();
    virtual ~Address();
    

    virtual const sockaddr* getAddr() const = 0;                    // 获取对应sockaddr
    virtual socklen_t getAddrLen() const = 0;                       // 获取sockaddr的大小   

    virtual std::ostream& insert(std::ostream& os) const;           // 把自己的数据插入到刘os中

    int getFamily() const;
    std::string toString();

    bool operator<(const Address& rhs) const;
    bool operator==(const Address& rhs) const;
    bool operator!= (const Address& rhs) const;
};



class IPAddress : public Address
{
public:
    using ptr = std::shared_ptr<IPAddress>;

    virtual IPAddress::ptr networkAddress(uint32_t prefix_len) = 0;
    virtual IPAddress::ptr subnetMask(uint32_t prefix_len) = 0;

    virtual uint32_t getPort() const = 0;
    virtual void setPort(uint32_t v) = 0;
};



class IPv4Address : public IPAddress
{
public:
    using ptr = std::shared_ptr<IPv4Address>;
    IPv4Address(const sockaddr_in& addr);
    IPv4Address(uint32_t address = INADDR_ANY, uint32_t port = 0);
    IPv4Address(const char* address, uint32_t port = 0);


    const sockaddr* getAddr() const override;
    socklen_t getAddrLen() const override;

    IPAddress::ptr broadcastAddress(uint32_t prefix_len);
    IPAddress::ptr networkAddress(uint32_t prefix_len) override;
    IPAddress::ptr subnetMask(uint32_t prefix_len) override;
    std::ostream& insert(std::ostream& os) const override;


    uint32_t getPort() const override;
    void setPort(uint32_t v) override;

private:
    sockaddr_in m_addr;         // 均以网络字节序存储
};



class IPv6Address : public IPAddress
{
public:
    using ptr = std::shared_ptr<IPv6Address>;
    IPv6Address(const sockaddr_in6& addr);
    IPv6Address(const char* address, uint32_t port = 0);
    IPv6Address(const uint8_t* addr, uint32_t port = 0);

    const sockaddr* getAddr() const override;
    socklen_t getAddrLen() const override;

    IPAddress::ptr networkAddress(uint32_t prefix_len) override;
    IPAddress::ptr subnetMask(uint32_t prefix_len) override;

    std::ostream& insert(std::ostream& os) const override;

    uint32_t getPort() const override;
    void setPort(uint32_t v) override;
    
private :
    sockaddr_in6 m_addr;
};



class UnixAddress : public Address
{
public:
    using ptr = std::shared_ptr<UnixAddress>;
    UnixAddress(const std::string& path);

    std::ostream& insert(std::ostream& os) const override;

    const sockaddr* getAddr() const override;
    socklen_t getAddrLen() const override;

private:
    sockaddr_un m_addr;
    socklen_t m_addr_size;
};



class UnknownAddress : public Address
{
public:
    using ptr = std::shared_ptr<UnknownAddress>;
    UnknownAddress(int family);

    std::ostream& insert(std::ostream& os) const override;

    const sockaddr* getAddr() const override;
    socklen_t getAddrLen() const override;

private:
    sockaddr m_addr;
};

}