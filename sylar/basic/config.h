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

    int addListener(ConfigData::on_change_cb cb);          // 添加更新回调
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
    static int LoadJson(const std::string& path, int config_id = 0);
    static int LoadJson(const char* path, int config_id = 0);

    template<typename ValueType>
    static ConfigVar<ValueType>::ptr LookUp(const std::string& path
            , const ValueType& default_value, const std::string& description = "", int config_id = 0, 
            typename ConfigVar<ValueType>::on_change_cb cb = nullptr){
        std::shared_lock<std::shared_mutex> rlock(getConfigRwMutex());
        std::cout << "LookUp: " << path << std::endl;
        return std::make_shared<ConfigVar<ValueType>>(path, default_value, description, config_id, cb);
    }

    // 使用静态函数返回静态成员变量，避免静态变量初始化顺序问题
    static ConfigData::ptr getConfigData(int config_id = 0){
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



const int getJsonValueByPath(const nlohmann::json& json_data, const std::string& path, nlohmann::json& value);

// 程序注册使用的单配置项
template<typename ValueType>
class ConfigVar {
public:
    using ptr = std::shared_ptr<ConfigVar>;
    using on_change_cb = std::function<void (const ValueType& new_val)>;

    ConfigVar (const std::string& path, const ValueType& default_value, const std::string& description, int config_id = 0,
               on_change_cb cb = nullptr)
            : m_path(path), m_description(description){
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
        
        // 添加监听器，监听配置项更新
        m_listen_key = ConfigManager::getConfigData(config_id)->addListener([this, path, cb](const nlohmann::json& new_val) {
                nlohmann::json json_data;
                int rt = getJsonValueByPath(new_val, path, json_data);
                if(rt == -1) {
                    std::cout << "[config error] config path " << path << " not found in json data -> ConfigVar::ConfigVar()" << std::endl;
                    return;
                }
                std::cout << "getConfigData : " <<  json_data.dump(4) << std::endl;
                ValueType new_value = FormatConversion<nlohmann::json, ValueType>()(json_data);
                setValue(new_value);
                if(cb) {
                    cb(new_value);
                }
        });
    }

    ~ConfigVar() {
        ConfigManager::getConfigData()->delListener(m_listen_key);
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
    int m_listen_key;               // config监听器key
    std::string m_description;
    ValueType m_val;
    std::shared_mutex m_mutex;
};

}