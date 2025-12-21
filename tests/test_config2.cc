#include "../sylar/config.h"
#include "../sylar/log.h"
#include <iostream>



void test1()
{

    YAML::Node root = YAML::LoadFile("/home/koddnty/user/projects/sylar/m_sylar/m_sylar/conf/basic.yaml");
    m_sylar::configManager::LoadFromYaml(root);
    auto test_logger = M_SYLAR_LOG_NAME("test_config");

    static m_sylar::ConfigVar<int>::ptr g_2tcp_connect_timeout =
        m_sylar::configManager::Lookup ("tcp.connect.timeout", 200, "tcp.connect.timeout");

    YAML::Node rroot = YAML::LoadFile("/home/koddnty/user/projects/sylar/m_sylar/m_sylar/conf/basic.yaml");
    m_sylar::configManager::LoadFromYaml(rroot);

    static m_sylar::ConfigVar<int>::ptr g_tcp_connect_timeout =
        m_sylar::configManager::Lookup<int> ("tcp.connect.timeout");

    if(!g_tcp_connect_timeout){
        M_SYLAR_LOG_ERROR(test_logger) << "g_tcp_connect_timeout is null";
    }

    M_SYLAR_LOG_DEBUG(test_logger) << "tcp.connect.timeout = " << g_tcp_connect_timeout->getValue();
    return;
}


int main(void)
{
    test1();
    return 0;
}