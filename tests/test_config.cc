#include "../sylar/config.h"
#include "../sylar/log.h"
#include <iostream>
#include <yaml-cpp/yaml.h>



// m_sylar::ConfigVar<int>::ptr system_port =
    // m_sylar::configManager::Lookup("system.port", (int)8080, "system port");

// m_sylar::ConfigVar<float>::ptr system_port2 =
//     m_sylar::configManager::Lookup("system.port", (float)8080, "system port");

// m_sylar::ConfigVar<std::vector<int>>::ptr system_int_vec =
//     m_sylar::configManager::Lookup("system.int_vec", std::vector<int> {1, 2}, "system port");
// m_sylar::ConfigVar<std::list<int>>::ptr system_int_list =
//     m_sylar::configManager::Lookup("system.int_list", std::list<int> {1, 2}, "system port");
// m_sylar::ConfigVar<std::set<int>>::ptr system_int_set =
//     m_sylar::configManager::Lookup("system.int_set", std::set<int> {1, 2}, "system port");
// m_sylar::ConfigVar<std::unordered_set<int>>::ptr system_int_unordered_set =
//     m_sylar::configManager::Lookup("system.int_unordered_set", std::unordered_set<int> {1, 2}, "system port");
// m_sylar::ConfigVar<std::map<std::string, int>>::ptr system_int_map =
//     m_sylar::configManager::Lookup("system.int_map", std::map<std::string, int> {{"kkxx", 2}, {"ggbb", 666}}, "system port");
// m_sylar::ConfigVar<std::unordered_map<std::string, int>>::ptr system_int_unordered_map =
//     m_sylar::configManager::Lookup("system.int_unordered_map", std::unordered_map<std::string, int> {{"unkkxx", 2}, {"unggbb", 666}}, "system port");
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
    YAML::Node root = YAML::LoadFile("/home/ls20241009/user/code/project/sylar_cp/m_sylar/conf/basic.yaml");
    M_SYLAR_LOG_ERROR(M_SYLAR_GET_LOGGER_ROOT()) << "root hello";

    
    print_yaml(root, 0);
}

// void test_config() {

#if 0
#define XX(g_var, name, prefix) \
    {   \
        auto& v = g_var->getValue(); \
        for(auto& it : v){  \
            M_SYLAR_LOG_INFO(M_SYLAR_GET_LOGGER_ROOT()) << #prefix "  " #name << it; \
        }   \
    }   

#define XX_MAP(g_var, name, prefix) \
    {   \
        auto& v = g_var->getValue(); \
        for(auto& it : v){  \
            M_SYLAR_LOG_INFO(M_SYLAR_GET_LOGGER_ROOT()) << #prefix "  " #name << " <===>  key   " << it.first << " |||| value " << it.second; \
        }   \
    }   

#endif
//     // XX(system_port, system_port, before);
//     // XX(system_port2, system_port2, before);
//     XX(system_int_vec, vec_int, before);
//     XX(system_int_list, list_int, before);
//     // XX(system_int_set, set_int, before);
//     XX(system_int_unordered_set, unordered_set_int, before);
//     XX_MAP(system_int_map, int_map, before);
//     XX_MAP(system_int_unordered_map, unordered_map, before);

//     std::cout << "1" << std::endl; 
//     std::cout << system_port->getValue() << "before" << std::endl;
//     std::cout << system_port2->getValue() << "before" << std::endl;
//     YAML::Node root = YAML::LoadFile("/home/ls20241009/user/code/project/sylar_cp/m_sylar/conf/basic.yaml");

//     std::cout << "1" << std::endl; 
//     m_sylar::configManager::LoadFromYaml(root);
//     std::cout << system_port->getValue() << "after" << std::endl;
//     std::cout << system_port2->getValue() << "after" << std::endl;
//     std::cout << "1" << std::endl; 

//     // XX(system_port, system_port, after);
//     // XX(system_port, system_port2, after);
//     XX(system_int_vec, vec_int, after);
//     XX(system_int_list, list_int, after);
//     // XX(system_int_set, set_int, after);
//     XX(system_int_unordered_set, unordered_set_int, after);
//     XX_MAP(system_int_map, int_map, after);
//     XX_MAP(system_int_unordered_map, unordered_map, after);

//     // M_SYLAR_LOG_INFO(M_SYLAR_GET_LOGGER_ROOT()) << "after system_int_set" << system_int_set->toString();


//     // auto& v = system_int_vec->getValue();
//     // for(auto& it : v){
//     //     M_SYLAR_LOG_INFO(M_SYLAR_GET_LOGGER_ROOT()) << it;
//     // }
// }

class test{
public:
    std::string name;
    int age;
    
    std::string toString() {
        std::stringstream ss;
        ss << "[name:" << name << ", age:" << age << "]";
        return ss.str();
    };
};

// void test_class(){
//     std::cout << "11" << std::endl;

//     test->addListener(10, [test](const std::string& old_val, const std::string& new_val){
//         M_SYLAR_LOG_INFO(M_SYLAR_GET_LOGGER_ROOT()) << "reload [" << test->getName() << "] from [" << old_val << "] to [" << new_val << "]";
//     });
//     std::cout << test->getValue() << "  before" << std::endl;
//     YAML::Node root = YAML::LoadFile("/home/ls20241009/user/code/project/sylar_cp/m_sylar/conf/basic.yaml");
//     m_sylar::configManager::LoadFromYaml(root);
//     std::cout << test->getValue() << "  after" << std::endl;
//     m_sylar::configManager::Print_all_conf();
// }

void test_config_log () {
    // Logger::ptr logger(new Logger("ali"));
    // LogAppender::ptr file_appender( new FileLogAppender("./log.txt"));
    // file_appender->setLevel(LogLevel::ERROR);
    // logger->addAppender(file_appender);
    // LoggerMgr::GetInstance()->addLogger(logger);
    // auto i = LoggerMgr::GetInstance()->getLogger("ali");
    // M_SYLAR_LOG_ERROR(i) << "instance";
    // M_SYLAR_LOG_ERROR(i) << "instance";
    // M_SYLAR_LOG_ERROR(i) << "instance";
    // M_SYLAR_LOG_ERROR(i) << "instance";
    // i = LoggerMgr::GetInstance()->getLogger("alii");
    // M_SYLAR_LOG_ERROR(i) << "instance222";
    // i = LoggerMgr::GetInstance()->getLogger("alii");
    // M_SYLAR_LOG_ERROR(i) << "instance222";

    // auto i = m_sylar::LoggerMgr::GetInstance()->getLogger("config");
    // auto i2 = m_sylar::LoggerMgr::GetInstance()->getLogger("system");
    // M_SYLAR_LOG_ERROR(i) << "instance1";
    // M_SYLAR_LOG_ERROR(i2) << "system1";
    YAML::Node loadYaml = YAML::LoadFile("/home/ls20241009/user/code/project/sylar_cp/m_sylar/conf/basic.yaml");
    // auto i = m_sylar::LoggerMgr::GetInstance()->getLogger("config");
    // auto i2 = m_sylar::LoggerMgr::GetInstance()->getLogger("system");
    m_sylar::configManager::LoadFromYaml(loadYaml);
    std::cout << "准备打印" << std::endl;
    // auto i = m_sylar::LoggerMgr::GetInstance()->getLogger("config");
    auto i2 = m_sylar::LoggerMgr::GetInstance()->getLogger("system2");
    // m_sylar::configManager::LoadFromYaml(loadYaml);
    // M_SYLAR_LOG_UNKNOWN(i) << "M_SYLAR_LOG_UNKNOWN";
    // M_SYLAR_LOG_DEGUB(i)   << "M_SYLAR_LOG_DEGUB";
    // M_SYLAR_LOG_INFO(i)    << "M_SYLAR_LOG_INFO";
    // M_SYLAR_LOG_WARN(i)    << "M_SYLAR_LOG_WARN";
    // M_SYLAR_LOG_ERROR(i)   << "M_SYLAR_LOG_ERROR";
    // M_SYLAR_LOG_FATAL(i)   << "M_SYLAR_LOG_FATAL";


    M_SYLAR_LOG_UNKNOWN(i2) << "M_SYLAR_LOG_UNKNOWN";
    M_SYLAR_LOG_DEGUB(i2) << "M_SYLAR_LOG_DEGUB";
    M_SYLAR_LOG_INFO(i2) << "M_SYLAR_LOG_INFO";
    M_SYLAR_LOG_WARN(i2) << "M_SYLAR_LOG_WARN";
    M_SYLAR_LOG_ERROR(i2) << "M_SYLAR_LOG_ERROR";
    M_SYLAR_LOG_FATAL(i2) << "M_SYLAR_LOG_FATAL";

    // i2->setFormatter("system 默认格式 %d%T%m%n");
    // M_SYLAR_LOG_UNKNOWN(i2) << "M_SYLAR_LOG_UNKNOWN";
    // M_SYLAR_LOG_DEGUB(i2) << "M_SYLAR_LOG_DEGUB";
    // M_SYLAR_LOG_INFO(i2) << "M_SYLAR_LOG_INFO";
    // M_SYLAR_LOG_WARN(i2) << "M_SYLAR_LOG_WARN";
    // M_SYLAR_LOG_ERROR(i2) << "M_SYLAR_LOG_ERROR";
    // M_SYLAR_LOG_FATAL(i2) << "M_SYLAR_LOG_FATAL";

    
    // M_SYLAR_LOG_DEGUB(i2) << "system2_debug";
    // m_sylar::configManager::Print_all_conf();

}

int main(void){
    
    // test_yaml();
    // test_config();
    // std::cout << "11" << std::endl;
    // test_class();
    std::cout << "111" << std::endl;
    test_config_log ();

    return 0;   
}