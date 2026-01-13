#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <fcntl.h>
#include <ifaddrs.h>
#include <map>
#include <memory>
#include <netdb.h>
#include <netinet/in.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include "address.h"
#include "endian.h"
#include "log.h"
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <utility>

namespace m_sylar{  

static Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

template<typename T>
T getMask(int prefix_len)
{
    static_assert(std::is_unsigned_v<T>);
    constexpr int bits = sizeof(T) * 8;
    // assert(bits >= prefix_len);
    // assert(prefix_len >= 0);
    if(bits <= prefix_len)
    {
        return ~T{0};
    }
    if(prefix_len <= 0)
    {
        return T{0};
    }

    return (~T{0}) << (bits - prefix_len); 
}

template<typename T>
static int countSetBites(T value)
{
    int count = 0;
    while(value)
    {
        count++;
        value &= (value - 1);
    }
    return count;
}

Address::Address() {}
Address::~Address() {}

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

Address::ptr Address::Create(sockaddr* addr, socklen_t path_len)
{
    if(!addr)
    {
        return nullptr;
    }
    Address::ptr result;
    switch (addr->sa_family) 
    {
        case AF_INET:
            result.reset(new IPv4Address(*(const sockaddr_in*)addr));
            break;
        case AF_INET6:
            result.reset(new IPv6Address(*(const sockaddr_in6*)addr));
            break;
        case AF_UNIX:
            result.reset(new UnixAddress(*(const sockaddr_un*)addr, path_len));
            break;
        default:
            result.reset(new UnknownAddress(*addr));
    }
    return result;
}

bool Address::Lookup(std::vector<Address::ptr>& result, const std::string& host, 
            int family, int type, int protocol)     // 域名，网络层协议，传输层类型，传输层协议
{   // 不支持url等协议到端口转换,baidu.com:http can work
    addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    addrinfo *results;
    hints.ai_flags = 0;
    hints.ai_family = family;
    hints.ai_socktype = type; 
    hints.ai_protocol = protocol;

    std::string node = "";
    std::string service = "";
    
    if(!host.empty() && host[0] == '[')
    {   // ipv6地址
        size_t end_node = host.find(']');
        if(end_node == std::string::npos)
        {
            M_SYLAR_LOG_ERROR(g_logger) << "invalid host : " << host;
            return false;
        }
        node = host.substr(1, end_node - 1);
        if(end_node + 2 < host.length())
        {
            service = host.substr(end_node + 2);
        }
    }
    
    if(node.empty())
    {   // ipv4地址 and host name
        size_t end_node = host.find(':');
        if(end_node == std::string::npos)
        {
            end_node = host.size();
        }
        node = host.substr(0, end_node);
        if(end_node + 1 < host.length())
        {
            service = host.substr(end_node + 1);
        }
    }

    int rt = getaddrinfo(node.c_str(), service.c_str(), &hints, &results);
    if(rt)
    {
        M_SYLAR_LOG_ERROR(g_logger) << "getaddrinfo failed, " << std::endl
                                    << "node : " << node 
                                    << "\nservice: " << service
                                    << "\nrt : " << rt
                                    << "\nerrno : " << errno << "error :" << strerror(errno);
        return false;
    }

    addrinfo* addr_node = results;
    while(addr_node)
    {
        result.push_back(Create(addr_node->ai_addr, addr_node->ai_addrlen));
        addr_node = addr_node->ai_next;
    }

    freeaddrinfo(results);
    if(result.size() <= 0)
    {
        return false;
    }
    return true;
}

Address::ptr Address::LookupAny(const std::string& host, int family,
                int type, int protocol)
{
    std::vector<Address::ptr> result;
    if(!Lookup(result, host, family, type, protocol))
    {
        return nullptr;
    }
    return result[0]; 
}

std::shared_ptr<IPAddress> Address::LookupAnyIPAddress(const std::string& host, int family,
            int type, int protocol)
{
    std::vector<Address::ptr> result;
    Lookup(result, host, family, type, protocol);
    for(auto& it : result)
    {
        IPAddress::ptr v = std::dynamic_pointer_cast<IPAddress>(it);
        if(v)
        {
            return v;
        }
    }
    return nullptr; 
}

bool Address::getInterfaceAddress(std::multimap<std::string, std::pair<Address::ptr, uint32_t>>& result, 
                int family)     // result has value : return true, rather false
{
    ifaddrs* interfaces;
    int rt = getifaddrs(&interfaces);
    if(rt)
    {
        M_SYLAR_LOG_ERROR(g_logger) << "failed to get interfaces information, \nerrno = " << errno
                                    << "\n e.what : " << strerror(errno)
                                    << "family = " << family;
        return false;
    }

    for(ifaddrs* p = interfaces; p; p = p->ifa_next)
    {
        if(family != AF_UNSPEC && p->ifa_addr->sa_family != family)
        {   // family condition not satisfied
            continue;
        }

        Address::ptr addr;
        int prefix_len = 0;

        if(p->ifa_addr->sa_family == AF_INET)
        {
            addr = Create(p->ifa_addr, sizeof(sockaddr_in));    // better transport thougt not used
            uint32_t netmask = ((sockaddr_in*)p->ifa_netmask)->sin_addr.s_addr;
            prefix_len = countSetBites(netmask);
        }
        else if(p->ifa_addr->sa_family == AF_INET6)
        {
            addr = Create(p->ifa_addr, sizeof(sockaddr_in6));
            uint8_t* netmask = ((sockaddr_in6*)p->ifa_netmask)->sin6_addr.s6_addr; 
            for(int i = 0; i < 16; i++)
            {
                prefix_len += countSetBites(netmask[i]);
            }
        }
        if(addr)
        {
            result.insert({p->ifa_name, {addr, prefix_len}});
        }
    }

    freeifaddrs(interfaces);
    return !result.empty();
}

bool Address::getInterfaceAddress(std::vector<std::pair<Address::ptr, uint32_t>>& result, 
                int family, const std::string& interfaceName)
{
    std::multimap<std::string, std::pair<Address::ptr, uint32_t>> all_result;

    if(!getInterfaceAddress(all_result, family))
    {
        return false;
    }

    auto fit_result = all_result.equal_range(interfaceName);
    for(auto it = fit_result.first; it != fit_result.second; ++it)
    {
        result.push_back(it->second);
    }

    return (!result.empty());
}

// IPv4Address 定义
IPv4Address::IPv4Address()
{
    m_addr.sin_family = AF_INET;
}

IPv4Address::IPv4Address(const sockaddr_in& addr)
{
    m_addr = addr;
}

IPv4Address::IPv4Address(uint32_t address, uint16_t port)
{
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin_family = AF_INET;
    m_addr.sin_addr.s_addr = byteSwapToBigEndian(address);
    m_addr.sin_port = byteSwapToBigEndian(port);
}

IPv4Address::IPv4Address(const char* address, uint16_t port)
{
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin_family = AF_INET;
    if(inet_pton(AF_INET, address, &m_addr.sin_addr.s_addr) != 1)
    {
        M_SYLAR_LOG_ERROR(g_logger) << "inet_ntop converse failed, errno = " << errno 
                << " " << strerror(errno); 
    }
    m_addr.sin_port = byteSwapToBigEndian(port);
}

sockaddr* IPv4Address::getAddr() const 
{
    return (sockaddr*)&m_addr;   
}

socklen_t IPv4Address::getAddrLen() const 
{
    return sizeof(m_addr);
}

IPAddress::ptr IPv4Address::broadcastAddress(uint32_t prefix_len)
{
    if(prefix_len > 32)
    {
        return nullptr;
    }
    sockaddr_in addr (m_addr);
    addr.sin_addr.s_addr |= byteSwapToBigEndian(~getMask<uint32_t>(prefix_len));
    return IPv4Address::ptr(new IPv4Address(addr));
}

IPAddress::ptr IPv4Address::networkAddress(uint32_t prefix_len) 
{
    if(prefix_len > 32)
    {
        return nullptr;
    }   
    sockaddr_in addr(m_addr);
    addr.sin_addr.s_addr &= byteSwapToBigEndian(getMask<uint32_t>(prefix_len));
    return IPv4Address::ptr(new IPv4Address(addr));
}


IPAddress::ptr IPv4Address::subnetMask(uint32_t prefix_len)  
{
    sockaddr_in mask;
    memset(&mask, 0, sizeof(sockaddr_in));
    mask.sin_family = AF_INET;
    mask.sin_addr.s_addr = byteSwapToBigEndian(getMask<uint32_t>(prefix_len));

    return IPAddress::ptr(new IPv4Address(mask));
}

std::ostream& IPv4Address::insert(std::ostream& os) const 
{
    uint32_t addr = m_addr.sin_addr.s_addr;
    uint16_t port = NetByteSwapToHostEndian(m_addr.sin_port);
    os  << ((addr >> 0) & 0xff) << "."
        << ((addr >> 8) & 0xff) << "."
        << ((addr >> 16) & 0xff) << "."
        << ((addr >> 24) & 0xff)
        << ":" << port;
    return os;
}

uint16_t IPv4Address::getPort() const  
{
    return NetByteSwapToHostEndian(m_addr.sin_port);
}

void IPv4Address::setPort(uint16_t v)  
{
   m_addr.sin_port = byteSwapToBigEndian(v);
}


// IPv6Address 定义
IPv6Address::IPv6Address()
{
    m_addr.sin6_family = AF_INET6;
}

IPv6Address::IPv6Address(const sockaddr_in6& addr)
{
    m_addr = addr;
}

IPv6Address::IPv6Address(const char* address, uint16_t port)
{   // 字符串地址
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin6_family = AF_INET6;
    if(inet_pton(AF_INET6, address, &m_addr.sin6_addr) != 1)
    {
        M_SYLAR_LOG_ERROR(g_logger) << "inet_ntop converse failed, errno = " << errno 
                << " " << strerror(errno); 
    }
    m_addr.sin6_port = byteSwapToBigEndian(port);
}

IPv6Address::IPv6Address(const uint8_t* addr, uint16_t port)
{   // 原生地址
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin6_family = AF_INET6;
    m_addr.sin6_port = byteSwapToBigEndian(port);
    memcpy(m_addr.sin6_addr.s6_addr, addr, 16); 
}


sockaddr* IPv6Address::getAddr() const 
{
    return (sockaddr*)&m_addr;
}

socklen_t IPv6Address::getAddrLen() const 
{
    return sizeof(m_addr);
}

IPAddress::ptr IPv6Address::networkAddress(uint32_t prefix_len) 
{
    sockaddr_in6 addr(m_addr);
    memset(addr.sin6_addr.s6_addr, 0, 16);
    int last_block6 = prefix_len / 8;
    int last_bits = prefix_len % 8;
    if(last_block6 >= 16)
    {
        last_block6 = 16;
        last_bits = 0;
    }

    for(int i = 0; i < last_block6; i++)
    {
        addr.sin6_addr.s6_addr[i] = m_addr.sin6_addr.s6_addr[i];
    }
    if(last_bits != 0)
    {
        addr.sin6_addr.s6_addr[last_block6] = (m_addr.sin6_addr.s6_addr[last_block6] >> last_bits) << last_bits;
    }
    return IPv6Address::ptr(new IPv6Address(addr));
}


IPAddress::ptr IPv6Address::subnetMask(uint32_t prefix_len)  
{
    sockaddr_in6 addr(m_addr);
    memset(addr.sin6_addr.s6_addr, 0, 16);
    int last_block6 = prefix_len / 8;
    int last_bits = prefix_len % 8;
    if(last_block6 >= 16)
    {
        last_block6 = 16;
        last_bits = 0;
    }

    for(int i = 0; i < last_block6; i++)
    {
        addr.sin6_addr.s6_addr[i] = ~(uint8_t {0});
    }
    if(last_bits != 0)
    {
        addr.sin6_addr.s6_addr[last_block6] = ~(uint8_t {0});
        addr.sin6_addr.s6_addr[last_block6] = (m_addr.sin6_addr.s6_addr[last_block6] >> last_bits) << last_bits;
    }
    return IPv6Address::ptr(new IPv6Address(addr));   
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

uint16_t IPv6Address::getPort() const  
{
    return NetByteSwapToHostEndian(m_addr.sin6_port);
}

void IPv6Address::setPort(uint16_t v)  
{
    m_addr.sin6_port = byteSwapToBigEndian(v);
}


// Unix定义
static const size_t MAX_PATH_LEN = sizeof(((sockaddr_un*)0)->sun_path) - 1;
UnixAddress::UnixAddress()
{
    m_addr.sun_family = AF_UNIX;
}

UnixAddress::UnixAddress(const std::string& path)
{   // 函数输入格式 抽象路径名 "\\0..."或 普通路径名 "..."
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sun_family = AF_UNIX;
    int path_len = path.length() + 1;    // 为string存储的字符串最后加入'\0'， 与 .c_str()同步

    if(!path.empty() && path[0] == '\0')
    {
        // 抽象路径名, 除去最后的'\0'
        path_len--;
    }

    if(path_len > MAX_PATH_LEN)
    {
        throw std::logic_error("path too long");
    }

    memcpy(m_addr.sun_path, path.c_str(), path_len);
    m_addr_size += offsetof(sockaddr_un, sun_path);
}

UnixAddress::UnixAddress(const sockaddr_un& addr, int path_len)
{
    m_addr_size = offsetof(sockaddr_un, sun_path) + path_len;
    m_addr = addr;
}

std::ostream& UnixAddress::insert(std::ostream& os) const 
{
    if(m_addr_size > offsetof(sockaddr_un, sun_path)
         && m_addr.sun_path[0] == '\0')
    {
        os << "\\0" << std::string(m_addr.sun_path + 1, m_addr_size - offsetof(sockaddr_un, sun_path) - 1);
        return os;
    }
    os << m_addr.sun_path;
    return os;
}

sockaddr* UnixAddress::getAddr() const 
{
    return (sockaddr*)&m_addr;
}

socklen_t UnixAddress::getAddrLen() const 
{
    return m_addr_size;
}



// known地址类型
UnknownAddress::UnknownAddress()
{    
}

UnknownAddress::UnknownAddress(int family)
{
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sa_family = family;
}

UnknownAddress::UnknownAddress(const sockaddr& addr)
{
    m_addr = addr;
}

std::ostream& UnknownAddress::insert(std::ostream& os) const 
{
    return os;
}


sockaddr* UnknownAddress::getAddr() const 
{
    return (sockaddr*)&m_addr;
}

socklen_t UnknownAddress::getAddrLen() const 
{
    return sizeof(m_addr);
}




}