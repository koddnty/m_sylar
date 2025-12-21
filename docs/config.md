# config模块 接口及配置方法

## config 模块描述

config配置项以外采用 **预设配置项** 模式，程序仅读取预设配置项目。一般情况下用户只能修改以下文档中明确列出的配置项，无法添加自定义字段。

用户可以自行添加更多的预设配置项目，参见[新增预设自定义配置项目](#新增预设自定义配置项目)

## config 模块yaml配置项

以下表示方法中，[0]代表是上级所属的一个数组元素，同级相同序号表示在一个map中。

### logs项

- ``logs``      (日志输出配置)
  - [0]``name``    (日志类名，可由GETLOGNAME宏获取)
  - [0]``level``   (日志级别)
  - [0]``appenders``   (日志输出器，定义当前日志输出类型以及输出位置和输出级别)
    - [0]``type``      (日志类型)
    - [0]``level``     (日志级别)
    - [0]``file``      (输出文件)

logs项实例:

``` yaml
log:
  - name: test_config
    level: DEBUG
    appenders:
      - type: FileLogAppender
        file: <path_to_output_file>
        level : DEBUG
      - type: StdoutLogAppender
        level: DEBUG
```

### tcp项

- ``tcp``   (tcp配置项)
  - [0]``connect`` (connect函数配置项)
    - [0]``timeout``     (tcp连接超时时间 uint64_t, 单位ms, 设定为-1则为不限制超时时间)

## 其他

### 新增预设自定义配置项目

若想使用config新增一个配置项目，则需要先在程序中声明出这个配置。如下:

```cpp
    static m_sylar::ConfigVar<int>::ptr new_config_instence =
        m_sylar::configManager::Lookup ("example", 200, "tcp.connect.timeout");

    YAML::Node rroot = YAML::LoadFile("<path_to_config>.yaml");
    m_sylar::configManager::LoadFromYaml(rroot);

    static m_sylar::ConfigVar<int>::ptr get_new_config_instence =
        m_sylar::configManager::Lookup<int> ("example");

```

程序在声明有new_config_instence配置项目后重新加载，才能使用。

此时，配置文件中example将被记录并能通过Lookup获取值,值的类型转化依赖boost库，通过模板类型设定配置类型如示例中，配置项将加载为int类型数据。

若想进一步添加特殊结构的节点，参考源码``struct LogDefine``做法。
