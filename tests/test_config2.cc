#include "basic/config.h"
#include "basic/log.h"
#include <iostream>



void test1()
{
    m_sylar::Logger::ptr logger = M_SYLAR_LOG_NAME("system");
    m_sylar::Logger::ptr t_logger = M_SYLAR_LOG_NAME("test_config");
    M_SYLAR_LOG_INFO(logger) << "begin test config"; ;
    std::string path = "/home/koddnty/user/projects/sylar/m_sylar/m_sylar/conf/test_config.json";
    auto test_logger = M_SYLAR_LOG_NAME("test_config");

    m_sylar::ConfigVar<std::string>::ptr g_test_student_name =
        m_sylar::ConfigManager::LookUp("test.student.name", std::string("liuyvqing"), 0, "test_student_name");

    M_SYLAR_LOG_INFO(t_logger) << "before test.student.name = " << g_test_student_name->getValue();
    m_sylar::ConfigManager::LoadJson(path, 0);
    M_SYLAR_LOG_INFO(logger) << "after loaded test.student.name = " << g_test_student_name->getValue();
    // t_logger = M_SYLAR_LOG_NAME("test_config");
    M_SYLAR_LOG_INFO(t_logger) << "after loaded test.student.name = " << g_test_student_name->getValue();
    
    std::string user_config = "/home/koddnty/user/projects/sylar/m_sylar/m_sylar/conf/local.json";
    m_sylar::ConfigManager::LoadJson(user_config, 1);
    m_sylar::ConfigVar<std::string>::ptr g_user_info =
        m_sylar::ConfigManager::LookUp("user.0.info", std::string("root(default)"), 1, "test_student_name");
    M_SYLAR_LOG_INFO(logger) << "user.0.info :" << g_user_info->getValue();

    return;
}

void test2()
{
    m_sylar::Logger::ptr t_logger = M_SYLAR_LOG_NAME("system");
    M_SYLAR_LOG_INFO(t_logger) << "before load test_config logger, stdout and build/log.txt";
    std::string path = "/home/koddnty/user/projects/sylar/m_sylar/m_sylar/conf/test_config.json";
    m_sylar::ConfigManager::LoadJson(path, 0);
    M_SYLAR_LOG_INFO(t_logger) << "after load test_config logger, stdout and log/system_log.txt";
}


int main(void)
{
    test1();
    return 0;
}