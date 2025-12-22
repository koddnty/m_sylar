#include "address.h"
#include <cstring>
#include <sstream>

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
    
}

const sockaddr* IPv4Address::getAddr() const 
{
    
}

socklen_t IPv4Address::getAddrLen() const 
{
    
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
    
}

uint32_t IPv4Address::getPort() const  
{
    
}

void IPv4Address::setPort(uint32_t v)  
{
    
}


// IPv6Address 定义
IPv6Address::IPv6Address(uint32_t address, uint32_t port)
{
    
}

const sockaddr* IPv6Address::getAddr() const 
{
    
}

socklen_t IPv6Address::getAddrLen() const 
{
    
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
    
}

uint32_t IPv6Address::getPort() const  
{
    
}

void IPv6Address::setPort(uint32_t v)  
{
    
}


}