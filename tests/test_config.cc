#include "../sylar/config.h"
#include "../sylar/log.h"
#include <iostream>
#include <yaml-cpp/yaml.h>



m_sylar::ConfigVar<int>::ptr system_port =
    m_sylar::configManager::Lookup("system.port", (int)8080, "system port");

m_sylar::ConfigVar<std::vector<int>>::ptr system_int_vec =
    m_sylar::configManager::Lookup("system.int_vec", std::vector<int> {1, 2}, "system port");
void print_yaml(const YAML::Node& node, int level){
    if(node.IsScalar()){
        std::cout << node.Scalar();
    }
    else if(node.IsNull()){
        return;
    }
    else if(node.IsMap()){
        for(auto& it : node){
            std::cout <<std::string(level * 2, ' ')
                << it.first << ": " ;
            print_yaml(it.second, level + 1 );
            std::cout << std::endl;
        }
    }
    else if(node.IsSequence()){
        for (size_t i = 0; i < node.size(); ++i){
            std::cout << std::string(level * 2, ' ') << "- ";
            print_yaml(node[i], level);
            std::cout << std::endl;
        }
    }
}

void test_yaml() {
    YAML::Node root = YAML::LoadFile("/home/ls20241009/user/code/project/sylar_cp/m_sylar/conf/log.yaml");
    M_SYLAR_LOG_ERROR(M_SYLAR_GET_LOGGER_ROOT()) << "root hello";

    
    print_yaml(root, 0);
}

void test_config() {
    M_SYLAR_LOG_INFO(M_SYLAR_GET_LOGGER_ROOT()) << "y" << system_port->getValue();
    M_SYLAR_LOG_INFO(M_SYLAR_GET_LOGGER_ROOT()) << "y" << system_port->toString();

    YAML::Node root = YAML::LoadFile("/home/ls20241009/user/code/project/sylar_cp/m_sylar/conf/log.yaml");
    m_sylar::configManager::LoadFromYaml(root);

    M_SYLAR_LOG_INFO(M_SYLAR_GET_LOGGER_ROOT()) << "o" << system_port->getValue();
    M_SYLAR_LOG_INFO(M_SYLAR_GET_LOGGER_ROOT()) << "o" << system_port->toString();


    auto& v = system_int_vec->getValue();
    for(auto& it : v){
        M_SYLAR_LOG_INFO(M_SYLAR_GET_LOGGER_ROOT()) << it;
    }
}

int main(void){
    
    // test_yaml();
    test_config();

    return 0;   
}