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

    int addListener(int key, ConfigData::on_change_cb cb);          // 添加更新回调
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
            , const ValueType& default_value, const std::string& description = "", int config_id = 0){
        std::shared_lock<std::shared_mutex> rlock(getConfigRwMutex());
        return std::make_shared<ConfigVar<ValueType>>(path, default_value, description, config_id);
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
    const T& operator() (const nlohmann::json& v){
        return v.get<T>();
    }
};


template<typename ValueType>
const ValueType& getJsonValueByPath(const nlohmann::json& json_data, const std::string& path, int& rt){
    int front_pos = 0, back_pos = 0;
    const nlohmann::json* current_node = &json_data;
    while(path.find('.', front_pos) != std::string::npos){
        back_pos = path.find('.', front_pos);
        std::string key = path.substr(front_pos, back_pos - front_pos);
        if(std::isdigit(key[0])) {
            int idx = std::stoi(key);
            if(idx < 0 || idx >= current_node->size()){
                rt = -1;
                return ValueType();
            }
            current_node = &((*current_node)[idx]);
        }
        else {
            if(current_node->find(key) == current_node->end()){
                rt = -1;
                return ValueType();
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
              rt = -1;
            return ValueType();
        }
        current_node = &((*current_node)[idx]);
        return current_node->get<ValueType>();
    }

    if(current_node->find(key) == current_node->end()){
        rt = -1;
        return ValueType();
    }
    current_node = &((*current_node)[key]);
    rt = 0;
    return current_node->get<ValueType>();
}


// 程序注册使用的单配置项
template<typename ValueType>
class ConfigVar {
public:
    using ptr = std::shared_ptr<ConfigVar>;

    ConfigVar (const std::string& path, const ValueType& default_value, const std::string& description, int config_id = 0)
            : m_path(path), m_description(description), m_val(default_value){
        
        ConfigManager::getConfigData(config_id)->addListener(0, [this, &path](const nlohmann::json& new_val) {
                int rt = 0;
                nlohmann::json json_data = getJsonValueByPath<nlohmann::json>(new_val, path, rt);
                if(rt == -1) {
                    std::cout << "[config error] config path " << path << " not found in json data => ConfigVar::ConfigVar()" << std::endl;
                    return;
                }
                ValueType new_value = FormatConversion<nlohmann::json, ValueType>()(json_data);
                setValue(new_value);
        });
    }

    ~ConfigVar() = default;

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
    std::string m_description;
    ValueType m_val;
    std::shared_mutex m_mutex;
};

}