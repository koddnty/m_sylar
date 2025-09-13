# m_sylar:c++协程库

sylar项目地址:[sylar服务器框架原地址](https://github.com/sylar-yin/sylar)

sylar视频地址:[[C++高级教程]从零开始开发服务器框架(sylar)](https://www.bilibili.com/video/BV184411s7qF)

---

## 日志系统

### 日志功能

实现全局规范化日志管理，便于问题定位。
通过为`logger`添加`event`来使用`logger`中的所有`appender`输出`formatter`中定义好的日志格式到`appender`对应的文件。

### 日志实现

- 1 . 流式日志风格  
- 2 . 日志格式自定义
- 3 . 日志级别
- 4 . 多日志分离
- 5 . 全局日志管理

[日志结构思维导图（.drawio）](docs/log_moudle)

### 使用方法

```cpp
    // file_appender->setFormatter(LogFormatter::ptr( new LogFormatter("%d{%Y-%m-%d %H:%M:%S}%T[%p]%n")));可以设置formatter的pattern日志输出格式

    Logger::ptr logger(new Logger("ali"));      \\logger名称
    LogAppender::ptr file_appender( new FileLogAppender("./log.txt"));   \\创建文件appender
    file_appender->setLevel(LogLevel::ERROR);       \\设置文件等级
    logger->addAppender(file_appender);             \\为logger添加appender
    LoggerMgr::GetInstance()->addLogger(logger);     \\将logger添加到全局manager
    auto i = LoggerMgr::GetInstance()->getLogger("ali");    \\获取名为ali的logger
    M_SYLAR_LOG_ERROR(i) << "instance";             \\流式输出日志
```

LogFormatter创建时自定义pattern日志格式支持的格式

```cpp
    // %m 输出代码中指定的日志信息
    // %p 日志信息输出级别，及 DEBUG,INFO,ERROR等
    // %r 累计毫秒数
    // %c 日志名称
    // %t 输出产生该日志的线程全名
    // %F 协程id
    // %d 时间
    // %F 输出日志消息产生时所在的文件名称
    // %L 输出代码中的行号
    // %n 换行
    // %s 普通字符串
    // %T tab
```

## config配置模块

模块添加了对各种数据的配置支持如基本变量,STL部分如`list`,`vector`,`unordered_set`,`set`,`unordered_map`,`map`的支持。并支持了yaml的日志导入功能。

- 1 .yaml格式配置导入  

``` cpp
    YAML::Node root = YAML::LoadFile("<</pathToConfig/*.yaml>>");
    m_sylar::configManager::LoadFromYaml(root);
```

- 2 .yaml配置命名规则

导入的配置名示例:

``` yaml
    logs:           //logs
    - name: root            //logs.0
        level: info
        formatter: "%d%T%m%n"
        appender:                   //logs.0.appender
        - type: FileLogAppender             //logs.0.appender.1
            file: log.txt                   //logs.0.appender.1.file
        - type: StdoutLogAppender
```

对应的file配置名则为 `logs.0.appender.0.file`

- 3 .日志变更监听回调功能

支持对日志项的配置变更回调函数

``` cpp
    m_sylar::ConfigVar<std::string>::ptr test =
        m_sylar::configManager::Lookup("logs.0.appender.0.file", std::string("HelloWorld"), "system port");
    test->addListener(10, [test](const std::string& old_val, const std::string& new_val){
        M_SYLAR_LOG_INFO(M_SYLAR_GET_LOGGER_ROOT()) << "reload [" << test->getName() << "] from [" << old_val << "] to [" << new_val << "]";
    });
```

调用addListener添加变更回调函数,日志改变会自动执行对应函数
