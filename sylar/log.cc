#include "log.h"

namespace m_sylar{

Logger::Logger(const std::string& name){
    m_name = name;
    m_formatter.reset(new LogFormatter("%d{%Y-%m-%d %H:%M:%S}%T[%p]%T[%c]%T[thread:%t fiber:%f]%T%F:%L%T%m%n"));
}

std::string LogLevel::to_string(LogLevel::Level level){
    switch (level){
#define XX(name) \
        case LogLevel::name: \
            return #name; \
            break;

    XX(DEBUG);
    XX(INFO);
    XX(WARN);
    XX(ERROR);
    XX(FATAL);
#undef XX
        default:
            return "UNKNOWN";
    }
}


void Logger::addAppender(LogAppender::ptr appender){
    if(!appender->getFormatter()){
        appender->setFormatter(m_formatter);
    }
    m_Appenders.push_back(appender);
}
void Logger::delAppender(LogAppender::ptr appender){
    m_Appenders.remove(appender);
}

LogEvent::LogEvent(const char* file, int32_t line, uint32_t elapse, 
    uint32_t threadID, uint32_t fiberID, uint64_t timer, std::shared_ptr<Logger> logger,
    LogLevel::Level level)
    : file_name(file), m_line(line), m_elapse(elapse),
      m_threadID(threadID), m_fiberID(fiberID),
      m_timer(timer), m_logger(logger), m_level(level){
}

LogEventWrap::LogEventWrap(std::shared_ptr<LogEvent> event)
    : m_event(event){
}
LogEventWrap::~LogEventWrap(){
    if(m_event){
        m_event->getLogger()->log(m_event->getLevel(), m_event);
    }
}
std::stringstream& LogEventWrap::getSS(){
    return m_event->getSS();
}

void Logger::log(LogLevel::Level level, LogEvent::ptr event){
    auto self = shared_from_this();
    if(level >= m_level){
        for(auto& it : m_Appenders){
            it->log(level, self , event);
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
void FileLogAppender::log(LogLevel::Level level, std::shared_ptr<m_sylar::Logger> logger, LogEvent::ptr event){
    if(level > m_level){
        m_file_stream << m_formatter->format(level, logger, event);
    } 
}
void StdoutLogAppender::log(LogLevel::Level level, std::shared_ptr<m_sylar::Logger> logger, LogEvent::ptr event){
    if(level > m_level){
        std::cout << m_formatter->format(level, logger, event);
    } 
}

LogFormatter::LogFormatter(const std::string& pattern)
    : m_pattern(pattern){
        init();
}

std::string LogFormatter::format(LogLevel::Level level , std::shared_ptr<Logger> logger, LogEvent::ptr event){
    std::stringstream ss;
    for(auto& i : m_items){
        i->format(ss, logger, level, event);
    }
    return ss.str();
}


//formatter子类items
class MessageFormatItem : public LogFormatter::formatterItem{
public:
    MessageFormatItem(const std::string& str = ""){}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override{
        os << event->getContent();
    }
};
class LevelFormatItem : public LogFormatter::formatterItem{
public:
    LevelFormatItem(const std::string& str = ""){}

    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override{
        os << LogLevel::to_string(level);
    }
};
class ElapseFormatItem : public LogFormatter::formatterItem{
public:
    ElapseFormatItem(const std::string& str = ""){}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override{
        os << event->getElapse();
    }
};
class NameFormatItem : public LogFormatter::formatterItem{
public:
    NameFormatItem(const std::string& str = ""){}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override{
        os << logger->getName();
    }
};
class threadFormatItem : public LogFormatter::formatterItem{
public:
    threadFormatItem(const std::string& str = ""){}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override{
        os << event->getThreadId();
    }
};
class fiberFormatItem : public LogFormatter::formatterItem{
public:
    fiberFormatItem(const std::string& str = ""){}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override{
        os << event->getFiberId();
    }
};
class DateTimeFormatItem : public LogFormatter::formatterItem{
public:
    DateTimeFormatItem (const std::string& format = "%Y:%m:%d %H:%M:&s")
        : m_format(format) {
    }
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override{
        os << event->getTimer();
    }
private: 
    std::string m_format;
};
class FilenameFormatItem : public LogFormatter::formatterItem{
public:
    FilenameFormatItem(const std::string& str = ""){}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override{
        os << event->getFileName();
    }
};
class LineFormatItem : public LogFormatter::formatterItem{
public:
    LineFormatItem(const std::string& str = ""){}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override{
        os << event->getLine();
    }
};
class NewLineFormatItem : public LogFormatter::formatterItem{
public:
    NewLineFormatItem(const std::string& str = ""){}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override{
        os << "\n";
    }
};
class stringFormatItem : public LogFormatter::formatterItem{
public:
    stringFormatItem(const std::string & str )
     :formatterItem(str),  m_str(str) {
    }
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override{
        os << m_str;
    }
private: 
    std::string m_str;
};
class TabFormatItem : public LogFormatter::formatterItem{
public:
    TabFormatItem(const std::string& str = ""){}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override{
        os << "\t";
    }
private: 
    std::string m_str;
};

//日志格式解析函数
void LogFormatter::init(){
    std::vector<std::tuple<std::string, std::string, int>> vec;     //连续字符串种类，格式记录
    std::string nstr;                                               //存储连续字符串
    int fmt_status = 0;         //字符状态
    for(size_t i = 0; i < m_pattern.size(); i++){
        if(m_pattern[i] == '%'){
            //添加上个vec记录
            if(!nstr.empty()){
                vec.push_back(std::make_tuple(nstr, std::string(), fmt_status));
            }
            //记录当前格式状态
            nstr = "";
            fmt_status = 1;
            //添加当前格式记录
            if ((++i) >= m_pattern.size()) {break;}
            //特殊格式
            // std::cout  << "特殊格式" << m_pattern[i] << std::endl;
            if(m_pattern[i] == 'd'){
                std::string time_str = "";
                // std::cout  << m_pattern[i + 1] << " "<< std::endl;
                if(m_pattern[i + 1] != '{' && (i + 1) < m_pattern.size()){
                    //后续无格式
                    // std::cout  << "后续无格式" << std::endl;
                    vec.push_back(std::make_tuple(std::string("d"), time_str, fmt_status));
                    //状态恢复
                    nstr = "";
                    fmt_status = 0;
                    continue;
                }
                //循环后续格式
                bool is_in = false;
                while ((i + 1) < m_pattern.size()){
                    ++i;
                    if(m_pattern[i] == '{'){
                        is_in = true;
                    }
                    time_str.push_back(m_pattern[i]);
                    if(is_in && m_pattern[i] == '}'){
                        //遇见}循环跳出
                        break;
                    }
                }
                if(i < m_pattern.size()){
                    vec.push_back(std::make_tuple(std::string("d"), time_str, fmt_status));
                }
            }
            else if(m_pattern[i] == '%'){
                vec.push_back(std::make_tuple(std::string(1, m_pattern[i]), std::string(), 0));
            }
            else if(isalpha(m_pattern[i])){
                //其他格式
                vec.push_back(std::make_tuple(std::string(1, m_pattern[i]), std::string(), fmt_status));
            }
            else{
                //未定义格式
                vec.push_back(std::make_tuple(std::string(1, m_pattern[i]), std::string(), 0));
            }
            //状态恢复
            nstr = "";
            fmt_status = 0;
        }
        else{
            nstr.push_back(m_pattern[i]);      //加到临时字符
        }
    }
    if(!nstr.empty()){
        vec.push_back(std::make_tuple(nstr, std::string(), fmt_status));
    }

    
    
    // %n 换行符，用来给输出的每条日志进行换行；
    // %r 输出自应用启动到输出该条Log信息所耗费的时间（以毫秒记）
    
    // %d 输出服务器的当前时间，默认格式为ISO8601（国际标准时间格式），也可以指定时间格式，如：%d{yyyy年MM月dd日 HH:mm:ss SSS}
    // %I 输出日志发生的位置，包括类名，线程，及在代码中的行数，如：Test.main（Test.java:10）
    
    
    // %% 输出一个"%"字符
    static std::map<std::string, std::function<formatterItem::ptr(const std::string& str)>> s_format_items = {
#define XX(str, C) \
        {#str, [](const std::string& string){ return formatterItem::ptr(new C(string)); }}

        XX(m, MessageFormatItem),           // %m 输出代码中指定的日志信息
        XX(p, LevelFormatItem),             // %p 日志信息输出级别，及 DEBUG,INFO,ERROR等
        XX(r, ElapseFormatItem),            // %r 累计毫秒数
        XX(c, NameFormatItem),              // %c 日志名称
        XX(t, threadFormatItem),            // %t 输出产生该日志的线程全名
        XX(f, fiberFormatItem),             // %F 协程id
        XX(d, DateTimeFormatItem),          // %d 时间
        XX(F, FilenameFormatItem),          // %F 输出日志消息产生时所在的文件名称
        XX(L, LineFormatItem),              // %L 输出代码中的行号
        XX(n, NewLineFormatItem),           // %n 换行
        XX(s, stringFormatItem),            // %s 普通字符串
        XX(T, TabFormatItem)                // %T tab
#undef XX
    };
    //添加到formatteritems中
    for(auto& it : vec){
        if(std::get<2>(it) == 0){
            //普通字符串
            m_items.push_back(formatterItem::ptr(new stringFormatItem(std::get<0>(it))));
        }
        else{
            auto itemType = s_format_items.find(std::get<0>(it));
            if(itemType == s_format_items.end()){
                m_items.push_back(formatterItem::ptr(new stringFormatItem("<<error_format %" + std::get<0>(it) + ">>")));
                m_error = true;
            }
            else {
                m_items.push_back(itemType->second(std::get<1>(it)));
            }
        }
        // std::cout << "[" << std::get<0>(it) << "]--[" << std::get<1>(it) << "]--" << "[" << std::get<2>(it) << "]" << std::endl;
    }
}

}