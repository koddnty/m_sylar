#include "log.h"
#include "config.h"

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

LogLevel::Level Level_FromString(const std::string& level_str){
#define XX(name) \
    if (level_str == #name) { \
        return LogLevel::name; \
    }
    XX(DEBUG);
    XX(INFO);
    XX(WARN);
    XX(ERROR);
    XX(FATAL);
    return LogLevel::UNKNOWN;
#undef XX
}



void Logger::addAppender(LogAppender::ptr appender){
    // if(!appender->getFormatter()){
    //     std::cout << "appender dose not have formatter " << std::endl;
    //     appender->setFormatter(m_formatter);
    // }
    std::unique_lock<std::shared_mutex> w_lock (m_rwMutex);
    m_Appenders.push_back(appender);
}
void Logger::delAppender(LogAppender::ptr appender){
    std::unique_lock<std::shared_mutex> w_lock (m_rwMutex);
    m_Appenders.remove(appender);
}
void Logger::clearAppenders(){
    std::unique_lock<std::shared_mutex> w_lock (m_rwMutex);
    m_Appenders.clear();
}

LogEvent::LogEvent(const char* file, int32_t line, uint32_t elapse,
    uint32_t threadID, const std::string& threadName, uint32_t fiberID, uint64_t timer, std::shared_ptr<Logger> logger,
    LogLevel::Level level)
    : file_name(file), m_line(line), m_elapse(elapse),
      m_threadID(threadID), m_threadName(threadName), m_fiberID(fiberID),
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
    // std::cout << level << "<<>>" << m_level << std::endl;
    if(level >= m_level){
        if(!m_Appenders.empty()){
            for(auto& it : m_Appenders){
                it->log(level, self , event);
            }
        }
        else if(m_root){
            std::cout << "没有appender,使用m_root" << std::endl;
            m_root->log(level, event);
        }
    }
}
// void Logger::debug(LogLevel::Level level, LogEvent::ptr event){
// }
// void Logger::info(LogLevel::Level level, LogEvent::ptr event){
// }
// void Logger::warn(LogLevel::Level level, LogEvent::ptr event){
// }
// void Logger::error(LogLevel::Level level, LogEvent::ptr event){
// }
// void Logger::fatal(LogLevel::Level level, LogEvent::ptr event){
// }

void Logger::setFormatter(const std::string& pattern) {
    std::unique_lock<std::shared_mutex> w_lock (m_rwMutex);
    LogFormatter::ptr new_formatter(new LogFormatter(pattern));
    if(m_formatter->isError()){
        std::cout << "Logger LogFormatter name=" << m_name
                  << "  pattern=" << pattern;
        return ;
    }
    m_formatter = new_formatter;
}


FileLogAppender::FileLogAppender(const std::string& file_name)
    : m_file_name(file_name){
        reopen();
    // std::cout << "will be written in " << m_file_name  << std::endl;
}
bool FileLogAppender::reopen(){
    if(m_file_stream){
        m_file_stream << std::flush;
        m_file_stream.close();
    }
    m_file_stream.open(m_file_name);
    return !!m_file_stream;
}
void FileLogAppender::log(LogLevel::Level level, std::shared_ptr<m_sylar::Logger> logger, LogEvent::ptr event){
    std::shared_lock<std::shared_mutex> r_lock (m_rwMutex);
    // std::cout << "FileLogAppender" << level << "<<>>" << m_level << std::endl;
    if(level >= m_level){
        if(m_formatter){
            m_file_stream << m_formatter->format(level, logger, event) << std::flush;
        }
        else{
            // std::cout << "logger formatter (default)" << std::endl;
            // m_file_stream << "logger->m_formatter->format(level, logger, event)" << std::flush;
            m_file_stream << logger->m_formatter->format(level, logger, event);
        }
    }
}
void StdoutLogAppender::log(LogLevel::Level level, std::shared_ptr<m_sylar::Logger> logger, LogEvent::ptr event){
    std::shared_lock<std::shared_mutex> r_lock (m_rwMutex);
    if(level >= m_level){
        if(m_formatter){
            std::cout << m_formatter->format(level, logger, event);
        }
        else{
            std::cout << logger->m_formatter->format(level, logger, event);
        }
    }
}

LogFormatter::LogFormatter(const std::string& pattern)
    : m_pattern(pattern){
        std::unique_lock<std::shared_mutex> w_lock (m_rwMutex);
        init();
}

std::string LogFormatter::format(LogLevel::Level level , std::shared_ptr<Logger> logger, LogEvent::ptr event){
    std::shared_lock<std::shared_mutex> r_lock (m_rwMutex);
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
        os << event->getLogger()->getName();
    }
};
class threadFormatItem : public LogFormatter::formatterItem{
public:
    threadFormatItem(const std::string& str = ""){}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override{
        os << event->getThreadId() << ":" << event->getThreadName();
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
    // "%d{%Y-%m-%d %H:%M:%S}%T[%p]%T[%c]%T[thread:%t fiber:%f]%T%F:%L%T%m%n"

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

LoggerManager::LoggerManager(){
    m_root.reset(new Logger);
    m_root->addAppender(StdoutLogAppender::ptr (new StdoutLogAppender));

    Logger::ptr system (new Logger ("system"));
    LogAppender::ptr file_appender( new FileLogAppender(getExecutableDir() + "/build/log.txt"));
    file_appender->setLevel(LogLevel::DEBUG);
    // file_appender->setFormatter()
    system->addAppender(file_appender);
    system->addAppender(StdoutLogAppender::ptr (new StdoutLogAppender));

    m_system = system;
    // LoggerMgr::GetInstance()->addLogger(m_root);
    // LoggerMgr::GetInstance()->addLogger(m_system);
}
Logger::ptr LoggerManager::getLogger(const std::string& name){
    {
        std::unique_lock<std::shared_mutex> ulock(m_rwMutex);
        const auto& it = m_loggers.find(name);
        // std::cout << "naem = " << name  << std::endl;
        // for(auto& it : m_loggers){
        //     std::cout << "m_loggers = " << it.first  << std::endl;
        // }
        // return (it == m_loggers.end()) ? m_root : it->second;
        if(it != m_loggers.end()){
            return it->second;
        }
    }
    {
        std::lock_guard<std::shared_mutex> guard(m_rwMutex);
        // 检查数据
        const auto& it = m_loggers.find(name);
        if(it != m_loggers.end()){
            return it->second;
        }
        // 为system提供初始化logger
        if(name == "system"){
            return m_system;
        }
        Logger::ptr logger (new Logger(name));
        std::cout << "供默认logger" << std::endl;
        // task ： 增添define。
        logger->m_root = m_root;        // 提供默认logger
        m_loggers[name] = logger;       // 添加到manager
        return logger;
    }
}
bool LoggerManager::addLogger (Logger::ptr logger){
    std::lock_guard<std::shared_mutex> guard(m_rwMutex);
    if(m_loggers.find(logger->getName()) == m_loggers.end()){
        m_loggers.insert({logger->getName(), logger});
        return true;
    }
    else{
        return false;
    }
}

//两个结构体与现有logger类对应
struct LogAppenderDefine{
    int type;                                           // 0.stdoutappender 1.fileappender,
    LogLevel::Level level = LogLevel::UNKNOWN;        // appender独有日志级别
    LogFormatter::ptr formatter;            // appender独有输出格式
    std::string file = "";                          //appender 输出位置

    bool operator== (const LogAppenderDefine& other) const{
        return level == other.level
                && file == other.file
                && other.formatter == formatter;
    }

};
struct LogDefine{
    std::string name;                         // 日志名
    LogLevel::Level level;                    // 日志输出最高级别
    LogFormatter::ptr formatter = nullptr;    // 日志默认输出格式
    std::vector<LogAppenderDefine> appenders;    //logger对应的appenders
    bool operator== (const LogDefine& other) const {
        return     other.name == name
                && other.level == level
                && other.formatter == formatter;
    }

    bool operator< (const LogDefine& other) const {
        return name < other.name;
    }
};
// void Logger::to_define(LogDefine& define, std::set<LogAppenderDefine> appenders){
//     LogAppenderDefine appender_struct = new LogAppenderDefine;
//     define.name = m_name;
//     define.level = m_level;
//     define.formatter = m_formatter;
//     define.appenders = appenders;
// }
//为logger结构体做偏特化
template<>
class LexicalCast<std::string, std::set<LogDefine>>{
public:
    std::set<LogDefine> operator() (const std::string& v){
        // std::cout << "LexicalCast<std::string, std::set<LogDefine>> 正在偏特化..." << std::endl;
        YAML::Node node =  YAML::Load(v);
        std::set<LogDefine> logDefine_set;
        // logger的解析
        for(const auto& it : node){
            if (!it["name"].IsDefined()){
                std::cout << "[log error] " << " logs.<i>.name为空 in LexicalCast<std::string, std::set<LogDefine>> :: operator()" << std::endl;
                continue;
            }
            LogDefine ld;                   // 新的ld
            ld.name = it["name"].Scalar();
            // std::cout << "it[\"level\"].Scalar()  " << it["level"].Scalar() << std::endl;
            ld.level = Level_FromString(it["level"].IsDefined() ? it["level"].Scalar() : "");
            // std::cout << "ld.level  " << LogLevel::to_string(ld.level) << std::endl;
            if(it["formatter"].IsDefined()){
                ld.formatter = std::shared_ptr<LogFormatter>(new LogFormatter(it["formatter"].Scalar()));
            }
            // appender的解析
            if(it["appenders"].IsDefined()){
                for(auto& appender : it["appenders"]){
                    // std::cout << "LexicalCast<std::string, std::set<LogDefine>> 正在偏特化... 添加appender..." << std::endl;

                    LogAppenderDefine new_appender ;
                    // 设置appender类型
                    if(!appender["type"].IsDefined()){
                        std::cout << "[log error] " << " logs.appenders.<i>.type in LexicalCast<std::string, std::set<LogDefine>> :: operator()" << std::endl;
                        continue;
                    }
                    std::string type = appender["type"].Scalar();
                    if(type == "StdoutLogAppender"){
                        new_appender.type = 0;
                    }
                    else if(type == "FileLogAppender" && appender["file"].IsDefined()){
                        new_appender.type = 1;
                        new_appender.file = appender["file"].Scalar();
                    }
                    else {
                        std::cout << "[log error] " << " unknown appenderType or appenderFile was not declared in LexicalCast<std::string, std::set<LogDefine>> :: operator()" << std::endl;
                        new_appender.type = -1;
                    }
                    // 设置appender formatter
                    if(appender["formatter"].IsDefined()){
                        new_appender.formatter = std::shared_ptr<LogFormatter>(new LogFormatter(appender["formatter"].Scalar()));
                    }
                    // 设置appender level
                    if(appender["level"].IsDefined()){
                        new_appender.level = Level_FromString(appender["level"].Scalar());
                    }
                    ld.appenders.push_back(new_appender);
                }
            }
            logDefine_set.insert(ld);
        }
        return logDefine_set;
    }
};
template<>
class LexicalCast<std::set<LogDefine>, std::string>{
public:
    std::string operator() (const std::set<LogDefine> & v){
        YAML::Node node;
        for (auto& logdefine : v){
            YAML::Node n;
            n["name"] = logdefine.name;
            n["level"] = LogLevel::to_string(logdefine.level);
            if(!logdefine.formatter){
                n["formatter"] = logdefine.formatter->getPattern();
            }
            for(auto& appender : logdefine.appenders){
                YAML::Node a;
                if(appender.type == 0){
                    a["type"] = "StdoutLogAppender";
                }
                else if(appender.type == 1){
                    a["type"] = "FileLogAppender";
                    a["file"] = appender.file;
                }
                else{
                    std::cout << "[log error]" << "appender does not have type, unknown error => LexicalCast<std::set<LogDefine>, std::string> :: operator()"  << std::endl;
                }
                a["level"] = LogLevel::to_string(logdefine.level);
                n["appenders"].push_back(a);
            }
        }
        // for(auto& it : v){
        //     node[it.first] = YAML::Node (LexicalCast<T, std::string>()(it.second));
        // }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};




//定义log日志配置
m_sylar::ConfigVar<std::set<LogDefine>>::ptr s_log_defines =
    m_sylar::configManager::Lookup ("logs", std::set<LogDefine>(), "log变量配置");
struct LogIniter
{
    //添加配置日志模块config变更logs日志监听函数，修改logger的manager变更日志
    LogIniter () {
        s_log_defines->addListener(0xA1B2C3, [](const std::set<LogDefine>& old_val, const std::set<LogDefine>& new_val){
            M_SYLAR_LOG_INFO(M_SYLAR_GET_LOGGER_ROOT()) << "s_log_defines changed";
            // 遍历日志中的的logger
            for (const auto& it : new_val){
                // std::cout  << "new_val  " << it.name << std::endl;
                auto jt = old_val.find(it);
                if(jt == old_val.end()){
                    //添加
                    // std::cout << "添加" << std::endl;
                    Logger::ptr logger(new Logger(it.name));
                    logger->setLevel(it.level);
                    // std::cout << "logger->setLevel(it.level)  " << LogLevel::to_string(it.level) << std::endl;
                    if(it.formatter){
                        logger->setFormatter(it.formatter);
                    }
                    else{

                    }
                    // std::cout << "it.appenders.size() = " << it.appenders.size() << std::endl;
                    for(auto& appender : it.appenders){
                        LogAppender::ptr appender_cast;
                        // 设置appender类型
                        if(appender.type == 0){
                            std::cout << "StdoutLogAppender" << std::endl;
                            appender_cast.reset(new StdoutLogAppender());
                        }
                        else if(appender.type == 1){
                            std::cout << "FileLogAppender " << "appender.file : " << appender.file << std::endl;
                            appender_cast.reset(new FileLogAppender(appender.file));
                        }
                        // 设置appender 自定义formatter
                        if(appender.formatter){
                            appender_cast->setFormatter(appender.formatter);
                        }
                        appender_cast->setLevel(appender.level);
                        // std::cout << "appender_cast->setLevel(appender.level)  " << LogLevel::to_string(appender.level) << std::endl;
                        logger->addAppender(appender_cast);
                    }
                    // std::cout << "logger->getName()" << logger->getName() << std::endl;
                    LoggerMgr::GetInstance()->addLogger(logger);        // 添加日志到manager
                }
                else if(!(*jt == it)){
                    std::cout  << "new_val  " << it.name << std::endl;
                    // 修改
                    // std::cout << "修改" << std::endl;
                    Logger::ptr logger = M_SYLAR_LOG_NAME(it.name);
                    logger->setLevel(it.level);
                    // std::cout << "logger->setLevel(it.level)  " << LogLevel::to_string(it.level) << std::endl;
                    if(it.formatter){
                        logger->setFormatter(it.formatter);
                    }
                    logger->clearAppenders();
                    for(auto& appender : it.appenders){
                        LogAppender::ptr appender_cast;
                        if(appender.type == 0){
                            std::cout << "StdoutLogAppender" << std::endl;
                            appender_cast.reset(new StdoutLogAppender());
                        }
                        else if(appender.type == 1){
                            std::cout << "FileLogAppender " << "appender.file : " << appender.file << std::endl;
                            appender_cast.reset(new FileLogAppender(appender.file));
                        }
                        // 设置appender 自定义formatter
                        if(appender.formatter){
                            appender_cast->setFormatter(appender.formatter);
                        }
                        appender_cast->setLevel(appender.level);
                        // std::cout << "appender_cast->setLevel(appender.level)  " << LogLevel::to_string(appender.level) << std::endl;

                        logger->addAppender(appender_cast);

                    }
                }
            }
            for (auto& it : old_val){
                const auto& jt = new_val.find(it);
                if(jt == new_val.end()){
                    //删除
                    // std::cout << "删除" << std::endl;
                    auto logger = M_SYLAR_LOG_NAME(it.name);
                    logger->setLevel(LogLevel::Level(100));
                    logger->clearAppenders();
                }
            }
        });
    }
};
static LogIniter __log_init;        // main函数前从config初始化log日志

void LoggerManager::init () {

}

}