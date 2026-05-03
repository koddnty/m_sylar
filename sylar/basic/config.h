#pragma once
#include <list>
#include <mutex>
#include <iostream>
#include <shared_mutex>
#include <nlohmann/json.hpp>
#include <boost/lexical_cast.hpp>
#include <charconv>


namespace m_sylar{
class ConfigManager;
#define M_SYALR_LOG_KEY 0


class ConfigData {
public:
    using ptr = std::shared_ptr<ConfigData>;
    using on_change_cb = std::function<void (const nlohmann::json& new_val)>;

    ConfigData() = default;
    ~ConfigData() = default;

    ConfigData(ConfigData&& other) ;
    ConfigData& operator= (ConfigData&& other);

    int setConfig(nlohmann::json&& json_data);              // 设置配置项，成功返回0
    int setConfig(const nlohmann::json& json_data);         // 设置配置项，成功返回0

    inline int addListener(ConfigData::on_change_cb cb) {
        int key = -1;
        std::unique_lock<std::shared_mutex> wlock(m_mutex);
        if(key == -1){
            key = ++m_max_listener_id;
        }
        m_cbs[key] = cb;
        return key;
    }
    int delListener(int key);                           // 删除更新回调             

    nlohmann::json& getJsonData() { std::shared_lock<std::shared_mutex> rlock(m_mutex); return m_json_data; }

private:
    nlohmann::json m_json_data;
    std::map<int, on_change_cb> m_cbs;
    int m_max_listener_id = 0;
    std::shared_mutex m_mutex;
};


template<typename ValueType>
class ConfigVar;
class ConfigManager{
public:
    static int LoadJson(const std::string& path, int config_id);
    static int LoadJson(const char* path, int config_id);


    /**
        @brief LookUp函数用来注册配置项，返回一个ConfigVar智能指针，用户可以通过这个指针获取配置项的值
            实现FormatConversion<nlohmann::json, ValueType>()来支持自定义类型的配置项，详细用法见源码日志模块处理日志配置项的例子
            需注意，错误的FormatConversion实现可能会导致程序崩溃，例如日志配置项的FormatConversion实现中，如果json格式错误或者缺少必要字段，程序会输出错误日志并返回一个空指针
            

        @param path 配置项路径，使用点分隔符，例如"test.student.0.name"
                    数字表示数组下标，例如"test.student.0.name"表示test.student数组下标为0的name字段
                    数字禁止用来表示名称如键是{"0" ： "value"} 将解析错误
        @param default_value 配置项默认值，如果在json文件中没有找到对应的配置项，将使用这个默认值
        @param description 配置项描述信息
        @param config_id 配置项所属配置id，默认为0，用户可以通过这个id来区分不同的配置项，例如用户可以定义一个日志配置id，另一个
                            配置项id用来区分其他配置项，用户也可以自定义更多的配置项id，例如数据库配置id等
        @param cb 配置项更新回调函数，当配置项的值发生变化时会调用这个函数，用户可以在这个函数中处理配置项更新后的逻辑，例如重新加载日志配置等
                    如 日志模块 的更新操作复杂，没有通过ConfigVar包装器更新，使用此方法更新也能在某些情况下避免getValue的锁竞争以提高并发性能

    */
    template<typename ValueType>
    static ConfigVar<ValueType>::ptr LookUp(const std::string& path
            , const ValueType& default_value, int config_id, const std::string& description = "", 
            typename ConfigVar<ValueType>::on_change_cb cb = nullptr){
        std::shared_lock<std::shared_mutex> rlock(getConfigRwMutex());
        // std::cout << "LookUp: " << path << std::endl;
        return std::make_shared<ConfigVar<ValueType>>(path, default_value, description, config_id, cb);
    }

    // 使用静态函数返回静态成员变量，避免静态变量初始化顺序问题
    static ConfigData::ptr getConfigData(int config_id){
        std::shared_lock<std::shared_mutex> wlock(getConfigRwMutex());
        static std::map<int, ConfigData::ptr> s_config_datas;
        if(s_config_datas.find(config_id) == s_config_datas.end()){
            s_config_datas[config_id] = std::make_shared<ConfigData> (ConfigData());
        }
        return s_config_datas[config_id];
    }
    static std::shared_mutex& getConfigRwMutex () {
        static std::shared_mutex config_rwMutex;
        return config_rwMutex;
    }
};


// convert json to configVar
// 模板函数用来做数据的类型转换
template<typename F, typename T>
class FormatConversion{
public:
    T operator() (const F& v){
        return boost::lexical_cast<T>(v);
    }
};

template<typename T>
class FormatConversion<nlohmann::json, T>{
public:
    T operator() (const nlohmann::json& v){
        return v.get<T>();
    }
};



inline int getJsonValueByPath(const nlohmann::json& json_data, const std::string& path, nlohmann::json& value) {
    int front_pos = 0, back_pos = 0;
    const nlohmann::json* current_node = &json_data;
    while(path.find('.', front_pos) != std::string::npos){
        back_pos = path.find('.', front_pos);
        std::string key = path.substr(front_pos, back_pos - front_pos);
        if(std::isdigit(key[0])) {
            int idx = std::stoi(key);
            if(idx < 0 || idx >= current_node->size()){
                return -1;
            }
            current_node = &((*current_node)[idx]);
        }
        else {
            if(current_node->find(key) == current_node->end()){
                return -1;
            }
            current_node = &((*current_node)[key]);
        }

        front_pos = back_pos + 1;
    }

    // 最后节点
    std::string key = path.substr(front_pos);
    if(std::isdigit(key[0])) {
        int idx = std::stoi(key);
        if(idx < 0 || idx >= current_node->size()){
                return -1;
        }
        current_node = &((*current_node)[idx]);
        value = *current_node;
        return 0;
    }

    if(current_node->find(key) == current_node->end()){
        return -1;
    }
    current_node = &((*current_node)[key]);
    value = *current_node;
    return 0;
}

// 程序注册使用的单配置项
template<typename ValueType>
class ConfigVar {
public:
    using ptr = std::shared_ptr<ConfigVar>;
    using on_change_cb = std::function<void (const ValueType& new_val)>;

    ConfigVar (const std::string& path, const ValueType& default_value, const std::string& description, int config_id,
               on_change_cb cb = nullptr)
            : m_path(path), m_description(description), m_config_id(config_id) {
        // 读取当前配置
        nlohmann::json json_data;
        int rt = getJsonValueByPath(ConfigManager::getConfigData(config_id)->getJsonData(), path, json_data);
        if(rt == -1) {
            // not found in json data, use default value
            m_val = default_value;
        }
        else {
            m_val = FormatConversion<nlohmann::json, ValueType>()(json_data);
        }
        if(cb) {
            cb(m_val);
        }
        
        // 添加监听器，监听配置项更新
        m_listen_key = ConfigManager::getConfigData(config_id)->addListener([this, path, cb](const nlohmann::json& new_val) {
                nlohmann::json json_data;
                int rt = getJsonValueByPath(new_val, path, json_data);
                if(rt == -1) {
                    std::cout << "[config error] config path " << path << " not found in json data -> ConfigVar::ConfigVar()" << std::endl;
                    return;
                }
                // std::cout << "getConfigData : " <<  json_data.dump(4) << std::endl;
                ValueType new_value = FormatConversion<nlohmann::json, ValueType>()(json_data);
                setValue(new_value);
                if(cb) {
                    cb(new_value);
                }
        });
    }

    ~ConfigVar() {
        ConfigManager::getConfigData(m_config_id)->delListener(m_listen_key);
    }

    int setValue(const ValueType& value) {
        std::unique_lock<std::shared_mutex> wlock(m_mutex);
        m_val = value;
        return 0;
    }

    const ValueType& getValue() {
        std::shared_lock<std::shared_mutex> rlock(m_mutex);
        return m_val;
    }

private:
    std::string m_path;
    int m_config_id;
    int m_listen_key;               // config监听器key
    std::string m_description;
    ValueType m_val;
    std::shared_mutex m_mutex;
};

}