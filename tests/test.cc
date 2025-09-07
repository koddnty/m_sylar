#include "../sylar/log.h"
#include <iostream>

using namespace m_sylar;
using namespace std;

int main(void){
    cout << "1" << endl;
    Logger::ptr logger(new Logger);
    cout << "1" << endl;
    logger->addAppender(LogAppender::ptr(new StdoutLogAppender));
    cout << "1" << endl;
    LogEvent::ptr event(new LogEvent(__FILE__, __LINE__, 0, 1, 2, time(0)));
    cout << "1" << endl;

    logger->log(LogLevel::DEBUG, event);
    // logger->format(LogLevel::DEBUG, logger, event);
    cout << "1" << endl;

    return 0;
}