#pragma once   

#include <string>
#include <memory>
#include <list>
#include <sstream>
#include <fstream>

namespace m_sylar{

class LogLevel{
public:
    enum Level{
        DEBUG = 1;
        INFO = 2;
        WARN = 3;
        ERROR = 4;
        FATAL = 5;
    };
};

//日志事件
class LogEvent{
public:
    typedef std::shared_ptr<LogEvent> ptr;
    LogEvent(){}

private:
    const char* file_name = nullptr;    //日志名
    int32_t m_line = 0;                 //行号
    int32_t m_threadID = 0;             //线程id
    int32_t m_fiberID = 0;              //协程id
    int32_t m_elapse = 0;               //程序启动时间（ms）
    int64_t m_timer;                    //时间戳
    std::string m_content;
};

//日志格式
class LogFormatter{
public:
    using ptr = std::shared_ptr<Formatter>;
    std::string format(LogEvent::prt event);
private:

};

//日志输出地
class LogAppender{
public:
    using ptr = std::shared_ptr<LogAppender> ;

    virtual ~LogAppender() {}

    
    virtual void log(LogLevel::Level level, LogEvent::ptr event) = 0;
    //formatter管理
    void setFormatter(LogFormatter::ptr val){m_formatter = val;}
    LogFormatter::ptr getFormatter(){return m_formatter;}
private:
    LogLevel::Level m_level;
    LogFormatter::ptr m_formatter;
};

//日志输出
class Logger{
public:
    typedef std::shared_ptr<Logger> ptr;


    void log(LogLevel::Level level, LogEvent::ptr event);

    //日志输出
    void debug(LogEvent::ptr event);
    void info(LogEvent::ptr event);
    void warn(LogEvent::ptr event);
    void error(LogEvent::ptr event);
    void fatal(LogEvent::ptr event);

    void addAppender(LogAppender::ptr appender);            //添加appender
    void delAppender(LogAppender::ptr appender);            //删除appender

    LogLevel::Level getLevel() const{return m_level;}
    void setLevel(LogLevel::Level val) {m_level = val;}
private:
    std::string m_name;
    LogLevel::Level m_level;
    std::list<LogAppender::ptr> m_Appenders;
};


//控制台日志
class StdoutLogAppender : public LogAppender{
public:
    typedef std::shared_ptr<StdoutLogAppender> ptr;
    void log(LogLevel::Level level, LogEvent::ptr event) override;

};
//文件日志
class FileLogAppender : public LogAppender{
public:
    FileLogAppender(const std::string& file_name);
    using ptr = std::shared_ptr<FileLogAppender>; 
    void log(LogLevel::Level level, LogEvent::ptr event) override;

    //文件打开函数，成功返回true
    bool reopen();
private:
    std::string  m_file_name;       //文件名
    std::ostream m_file_stream;     //文件流
};

}