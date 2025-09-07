#include "../sylar/log.h"
#include "../sylar/until.h"
#include <iostream>
#include <unistd.h> 
#include <pthread.h>
#include <functional>

using namespace m_sylar;
using namespace std;

int main(void){
    Logger::ptr logger(new Logger);
    logger->addAppender(LogAppender::ptr(new StdoutLogAppender));
    logger->addAppender(LogAppender::ptr(new StdoutLogAppender));
    // LogEvent::ptr event(new LogEvent(__FILE__, __LINE__, 0, getThreadId(),  2, time(0), logger, LogLevel::DEBUG));
    // logger->log(LogLevel::DEBUG, event);
    // logger->format(LogLevel::DEBUG, logger, event);
    cout << "Hello_m_sylar" << endl;
    
    M_SYLAR_LOG_UNKNOWN(logger) << "我爱中国1";
    M_SYLAR_LOG_DEGUB(logger) << "我爱中国2";
    M_SYLAR_LOG_WARN(logger) << "我爱中国3";
    M_SYLAR_LOG_ERROR(logger) << "我爱中国4";
    M_SYLAR_LOG_FATAL(logger) << "我爱中国5";




    return 0;
}