#include "log.h"

namespace m_sylar{

Logger::Logger(const std::string& name = "root"){
    m_name = name;

}


void Logger::addAppender(LogAppender::ptr appender){
    m_appenders.push_back(appender);
}
void Logger::delAppender(LogAppender::ptr appender){
    m_appenders.remove(appender);
}


void Logger::log(LogLevel::Level level, LogEvent::ptr event){
    if(level > m_level){
        for(auto& it : m_appenders){
            it->log(level, event);
        }
    }
}
void Logger::debug(LogLevel::Level level, LogEvent::ptr event){
}
void Logger::info(LogLevel::Level level, LogEvent::ptr event){
}
void Logger::warn(LogLevel::Level level, LogEvent::ptr event){
}
void Logger::error(LogLevel::Level level, LogEvent::ptr event){
}
void Logger::fatal(LogLevel::Level level, LogEvent::ptr event){
}




FileLogAppender::FileLogAppender(const std::string& file_name)
    : m_file_name(file_name){
}
bool FileLogAppender::reopen(){
    if(m_file_stream){
        m_file_stream.close();
    }
    m_file_stream.open(m_file_name);
    return !!m_file_stream;
}
void FileLogAppender::log(LogLevel::Level level, LogEvent::ptr event){
    if(level > m_level){
        m_file_stream << m_formatter->format(event);
    } 
}
void StdoutLogAppender::log(LogLevel::Level level, LogEvent::ptr event){
    if(level > m_level){
        std::cout << m_formatter->format(event);
    } 
}




}