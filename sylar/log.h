#pragma once   
#include "singleton.h"
#include <string>
#include <vector>
#include <memory>
#include <list>
#include <map>
#include <functional>
#include <sstream>
#include <fstream>
#include <iostream>

#define M_SYLAR_LOG_EVENT(logger, level)\
    if(logger->getLevel() <= level) \
        m_sylar::LogEventWrap(m_sylar::LogEvent::ptr(new LogEvent(__FILE__, __LINE__, 0 \
                    , getThreadId(),  2, time(0), logger, level))).getSS()

#define M_SYLAR_LOG_UNKNOWN(logger) M_SYLAR_LOG_EVENT(logger, LogLevel::UNKNOWN)
#define M_SYLAR_LOG_DEGUB(logger)   M_SYLAR_LOG_EVENT(logger, LogLevel::DEBUG)
#define M_SYLAR_LOG_INFO(logger) M_SYLAR_LOG_EVENT(logger, LogLevel::INFO)
#define M_SYLAR_LOG_WARN(logger) M_SYLAR_LOG_EVENT(logger, LogLevel::WARN)
#define M_SYLAR_LOG_ERROR(logger) M_SYLAR_LOG_EVENT(logger, LogLevel::ERROR)
#define M_SYLAR_LOG_FATAL(logger) M_SYLAR_LOG_EVENT(logger, LogLevel::FATAL)


namespace m_sylar{


class Logger;

class LogLevel{
public:
    using ptr = std::shared_ptr<LogLevel>;
    enum Level{
        UNKNOWN = 0,
        DEBUG = 1,
        INFO = 2,
        WARN = 3,
        ERROR = 4,
        FATAL = 5
    };

    static std::string to_string(LogLevel::Level level);
};

//日志事件
class LogEvent{
public:
    typedef std::shared_ptr<LogEvent> ptr;
    LogEvent(const char* file, int32_t m_line, uint32_t elapse, uint32_t threadID,
            uint32_t fiberID, uint64_t timer, std::shared_ptr<Logger> logger, 
            LogLevel::Level level);

    const char* getFileName() const{ return file_name;}
    
    int32_t getLine() const{ return m_line;}
    uint32_t getThreadId() const{ return m_threadID;}
    uint32_t getFiberId() const{ return m_fiberID;}
    uint32_t getElapse() const{ return m_elapse;}
    uint32_t getTimer() const{ return m_timer;}
    std::shared_ptr<Logger> getLogger() const{ return m_logger;}
    std::string getContent () const{ return m_ss.str();}
    LogLevel::Level getLevel () const{ return m_level;}
    std::stringstream& getSS () { return m_ss;}
    // void format();
private:
    const char* file_name = nullptr;    //日志名
    int32_t m_line = 0;                 //行号
    uint32_t m_elapse = 0;               //程序启动时间（ms）
    uint32_t m_threadID = 0;             //线程id
    uint32_t m_fiberID = 0;              //协程id
    uint64_t m_timer;                    //时间戳
    std::stringstream m_ss;              //日志格式
    std::shared_ptr<Logger> m_logger;    //所属日志
    LogLevel::Level m_level;             //日志等级 
};

class LogEventWrap{
public:
    LogEventWrap(std::shared_ptr<LogEvent> event);
    ~LogEventWrap();
    std::shared_ptr<LogEvent> getEvent() const {return m_event;}
    std::stringstream& getSS ();    
private: 
    std::shared_ptr<LogEvent> m_event;
};

//日志格式
class LogFormatter{
public:
    using ptr = std::shared_ptr<LogFormatter>;

    LogFormatter(const std::string& pattern);

    std::string format(LogLevel::Level level,  std::shared_ptr<Logger> logger, LogEvent::ptr event);

public:
    void init();            //初始化解析日志格式            
    //格式items
    class formatterItem{
    public:
        formatterItem(const std::string& format = "") {}
        using ptr = std::shared_ptr<formatterItem>;
        virtual ~formatterItem() {}
        virtual void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level , LogEvent::ptr event) = 0; 
    };
private:
    std::string m_pattern;
    std::vector<formatterItem::ptr> m_items;
    bool m_error = false;
};


//日志输出地
class LogAppender{
public:
    using ptr = std::shared_ptr<LogAppender> ;

    virtual ~LogAppender() {}

    
    virtual void log(LogLevel::Level level, std::shared_ptr<m_sylar::Logger> logger, LogEvent::ptr event) = 0;
    //formatter管理
    void setFormatter(LogFormatter::ptr val){m_formatter = val;}
    void setLevel(LogLevel::Level level){ m_level = level;}
    LogFormatter::ptr getFormatter(){return m_formatter;}
protected:
    LogLevel::Level m_level = LogLevel::UNKNOWN;
    LogFormatter::ptr m_formatter;

};


//日志输出
class Logger : public std::enable_shared_from_this<Logger>{
public:
    using ptr = std::shared_ptr<Logger>;

    Logger(const std::string& name = "root");

    void log(LogLevel::Level level, LogEvent::ptr event);

    //日志输出
    void debug(LogLevel::Level level, LogEvent::ptr event);
    void info(LogLevel::Level level, LogEvent::ptr event);
    void warn(LogLevel::Level level, LogEvent::ptr event);
    void error(LogLevel::Level level, LogEvent::ptr event);
    void fatal(LogLevel::Level level, LogEvent::ptr event);

    void addAppender(LogAppender::ptr appender);            //添加appender
    void delAppender(LogAppender::ptr appender);            //删除appender

    LogLevel::Level getLevel() const{return m_level;}
    void setLevel(LogLevel::Level val) {m_level = val;}

    std::string getName() const {return m_name;}
private:
    std::string m_name;
    LogLevel::Level m_level;
    std::list<LogAppender::ptr> m_Appenders;
    LogFormatter::ptr m_formatter;
};

//控制台日志
class StdoutLogAppender : public LogAppender{
public:
    typedef std::shared_ptr<StdoutLogAppender> ptr;
    void log(LogLevel::Level level, std::shared_ptr<m_sylar::Logger> logger, LogEvent::ptr event) override;

};

//文件日志
class FileLogAppender : public LogAppender{
public:
    FileLogAppender(const std::string& file_name);
    using ptr = std::shared_ptr<FileLogAppender>; 
    void log(LogLevel::Level level, std::shared_ptr<Logger> logger, LogEvent::ptr event) override;

    //文件打开函数，成功返回true
    bool reopen();
private:
    std::string  m_file_name;       //文件名
    std::ofstream m_file_stream;     //文件流
};


class LoggerManager{
public:
    LoggerManager();
    Logger::ptr getLogger (const std::string& name);
    bool addLogger (Logger::ptr logger);
private:
    std::map<std::string, Logger::ptr> m_loggers;
    Logger::ptr m_root;
};

typedef m_sylar::Singleton<LoggerManager> LoggerMgr;

}