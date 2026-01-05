#include "basic/address.h"
#include "basic/log.h"
#include <cstdint>
#include <iostream>
#include <map>
#include <sys/socket.h>

static m_sylar::Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

void test_interface()
{
    std::multimap<std::string, std::pair<m_sylar::Address::ptr, uint32_t>> interfaceAddress;
    int rt = m_sylar::Address::getInterfaceAddress(interfaceAddress, AF_UNSPEC);
    if(!rt)
    {
        M_SYLAR_LOG_ERROR(g_logger) << "failed to get any interface address";
    }

    for(auto it : interfaceAddress)
    {
        M_SYLAR_LOG_INFO(g_logger) << ">> " << it.first << "   "
                                   << it.second.first->toString() << "   " 
                                   << it.second.second; 
    }
}

void test_Lookup()
{
    std::vector<m_sylar::Address::ptr> result;
    std::string host = "baidu.com:http";
    m_sylar::Address::Lookup(result, host);
    for(auto& it : result)
    {
        std::cout << it->toString() << std::endl; 
    }
}

int main(void)
{
    test_Lookup();
    return 0;
}