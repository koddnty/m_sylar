#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <sys/socket.h>
#include "address.h"
#include "endian.h"
#include <arpa/inet.h>

namespace m_sylar{  

int Address::getFamily() const
{
    return getAddr()->sa_family;
}

std::string Address::toString()
{
    std::stringstream ss;
    insert(ss);
    return ss.str();
}

bool Address::operator<(const Address& rhs) const
{
    int minLen = (this->getAddrLen() < rhs.getAddrLen()) ? getAddrLen() : rhs.getAddrLen();
    int cmp_result = memcmp(getAddr(), rhs.getAddr(), minLen);

    if(cmp_result < 0)
    {
        return true;
    }
    else if(cmp_result > 0)
    {
        return false;
    }
    else if(getAddrLen() < rhs.getAddrLen())
    {
        return true;
    }
    return false;
}

bool Address::operator==(const Address& rhs) const
{
    return getAddrLen() == rhs.getAddrLen() &&
        memcmp(getAddr(), rhs.getAddr(), getAddrLen());
}

bool Address::operator!= (const Address& rhs) const
{
    return !(getAddrLen() == rhs.getAddrLen() &&
        memcmp(getAddr(), rhs.getAddr(), getAddrLen()));
}


// IPv4Address 定义
IPv4Address::IPv4Address(uint32_t address, uint32_t port)
{
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin_family = AF_INET;
    m_addr.sin_addr.s_addr = byteSwapToBigEndian(address);
    m_addr.sin_port = byteSwapToBigEndian(port);
}

const sockaddr* IPv4Address::getAddr() const 
{
    return (sockaddr*)&m_addr;   
}

socklen_t IPv4Address::getAddrLen() const 
{
    return sizeof(m_addr);
}

IPAddress::ptr IPv4Address::broadcastAddress(uint32_t prefix_len)
{
       
}

IPAddress::ptr IPv4Address::networkAddress(uint32_t prefix_len) 
{
    
}


IPAddress::ptr IPv4Address::subnetMask(uint32_t prefix_len)  
{
    
}

std::ostream& IPv4Address::insert(std::ostream& os) const 
{
    uint32_t addr = m_addr.sin_addr.s_addr;
    uint32_t port = NetByteSwapToHostEndian(m_addr.sin_port);
    os  << ((addr >> 24) & 0xff) << "."
        << ((addr >> 16) & 0xff) << "."
        << ((addr >> 8) & 0xff) << "."
        << ((addr >> 0) & 0xff) << ":" 
        << ":" << port;
    return os;
}

uint32_t IPv4Address::getPort() const  
{
    return NetByteSwapToHostEndian(m_addr.sin_port);
}

void IPv4Address::setPort(uint32_t v)  
{
   m_addr.sin_port = byteSwapToBigEndian(v);
}


// IPv6Address 定义
IPv6Address::IPv6Address(const char* address, uint32_t port)
{
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin6_family = AF_INET6;
    m_addr.sin6_port = byteSwapToBigEndian(port);
    memcpy(&m_addr.sin6_addr.s6_addr, address, 16);
}

const sockaddr* IPv6Address::getAddr() const 
{
    return (sockaddr*)&m_addr;
}

socklen_t IPv6Address::getAddrLen() const 
{
    return sizeof(m_addr);
}

IPAddress::ptr IPv6Address::broadcastAddress(uint32_t prefix_len)
{
    
}

IPAddress::ptr IPv6Address::networkAddress(uint32_t prefix_len) 
{
    
}


IPAddress::ptr IPv6Address::subnetMask(uint32_t prefix_len)  
{
    
}

std::ostream& IPv6Address::insert(std::ostream& os) const  
{
    char buffer[INET6_ADDRSTRLEN];  // 46字节足够
    
    // 将IPv6地址转换为字符串
    const char* result = inet_ntop(AF_INET6, 
                                   &m_addr.sin6_addr, 
                                   buffer, 
                                   sizeof(buffer));
    os << "[" << buffer << "]" << ":" << NetByteSwapToHostEndian(m_addr.sin6_port);
    return os;
}

uint32_t IPv6Address::getPort() const  
{
    return NetByteSwapToHostEndian(m_addr.sin6_port);
}

void IPv6Address::setPort(uint32_t v)  
{
    m_addr.sin6_port = byteSwapToBigEndian(v);
}


// Unix定义
UnixAddress::UnixAddress(const std::string& path)
{

}

std::ostream& UnixAddress::insert(std::ostream& os) const 
{

}

const sockaddr* UnixAddress::getAddr() const 
{

}

socklen_t UnixAddress::getAddrLen() const 
{

}

}