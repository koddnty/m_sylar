#include "../sylar/log.h"
#include "../sylar/until.h"
#include <iostream>
#include <unistd.h> 
#include <pthread.h>
#include <functional>

using namespace m_sylar;
using namespace std;

int main(void){
    // Logger::ptr logger(new Logger);
    // logger->addAppender(LogAppender::ptr(new StdoutLogAppender));
    // logger->addAppender(LogAppender::ptr(new StdoutLogAppender));
    // LogAppender::ptr file_appender( new FileLogAppender("/home/ls20241009/user/code/project/sylar_cp/m_sylar/build/log.txt"));
    // file_appender->setFormatter(LogFormatter::ptr( new LogFormatter("%d{%Y-%m-%d %H:%M:%S}%T[%p]偶麻辣买啦卡%n")));
    // file_appender->setLevel(LogLevel::ERROR);
    // logger->addAppender(file_appender);
    // cout << "Hello_m_sylar" << endl;
    
    // M_SYLAR_LOG_UNKNOWN(logger) << "我爱中国1";
    // M_SYLAR_LOG_DEBUG(logger) << "我爱中国2";
    // M_SYLAR_LOG_WARN(logger) << "我爱中国33";
    // M_SYLAR_LOG_ERROR(logger) << "我爱中国44";
    // M_SYLAR_LOG_FATAL(logger) << "我爱中国55";

    Logger::ptr logger(new Logger("ali"));
    LogAppender::ptr file_appender( new FileLogAppender("./log.txt"));
    file_appender->setLevel(LogLevel::ERROR);
    LogFormatter::ptr formatter ( new LogFormatter("测试pattern %m%n "));
    // std::string pattern = "测试pattern %m%n ";
    file_appender->setFormatter(formatter);
    logger->addAppender(file_appender);
    LoggerMgr::GetInstance()->addLogger(logger);
    auto i = LoggerMgr::GetInstance()->getLogger("ali");
    M_SYLAR_LOG_ERROR(i) << "instance";
    // M_SYLAR_LOG_ERROR(i) << "instance";
    // M_SYLAR_LOG_ERROR(i) << "instance";
    // M_SYLAR_LOG_ERROR(i) << "instance";
    // // Logger::ptr logger2(new Logger("alii"));
    // // LogAppender::ptr file_appender( new FileLogAppender("./log.txt"));
    // // file_appender->setLevel(LogLevel::ERROR);
    // // logger->addAppender(file_appender);
    // // LoggerMgr::GetInstance()->addLogger(logger2);
    // i = LoggerMgr::GetInstance()->getLogger("alii");
    // M_SYLAR_LOG_ERROR(i) << "instance222";
    // i = LoggerMgr::GetInstance()->getLogger("alii");
    // M_SYLAR_LOG_ERROR(i) << "instance222";
    return 0;
}