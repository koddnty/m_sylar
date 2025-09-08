#include "../sylar/config.h"
#include "../sylar/log.h"
#include <iostream>

m_sylar::ConfigVar<int>::ptr server_port =
    m_sylar::configManager::Lookup("port", (int)8080, "system port");
int main(void){
    M_SYLAR_LOG_INFO(M_SYLAR_GET_LOGGER_ROOT()) << server_port->getValue();
    M_SYLAR_LOG_INFO(M_SYLAR_GET_LOGGER_ROOT()) << server_port->toString();
    return 0;
}