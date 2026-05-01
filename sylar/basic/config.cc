#include "config.h"
#include <fstream>

namespace m_sylar {


ConfigData::ConfigData(ConfigData&& other)  {
    if(this == &other){
        return;
    }
    std::unique_lock<std::shared_mutex> twlock (this->m_mutex);
    std::shared_lock<std::shared_mutex> owlock (other.m_mutex);

    this->m_json_data = other.m_json_data;
    this->m_cbs = other.m_cbs;
    this->m_max_listener_id = other.m_max_listener_id;
    other.m_cbs.clear();
    other.m_max_listener_id = -1;
    other.m_json_data.clear();
}

ConfigData& ConfigData::operator= (ConfigData&& other) {
    if(this == &other){
        return *this;
    }
    std::unique_lock<std::shared_mutex> twlock (this->m_mutex);
    std::shared_lock<std::shared_mutex> owlock (other.m_mutex);

    this->m_json_data = other.m_json_data;
    this->m_cbs = other.m_cbs;
    this->m_max_listener_id = other.m_max_listener_id;
    other.m_cbs.clear();
    other.m_max_listener_id = -1;
    other.m_json_data.clear();

    return *this;
}


int ConfigData::setConfig(nlohmann::json&& json_data) {
    std::unique_lock<std::shared_mutex> wlock(m_mutex);
    m_json_data = std::move(json_data);
    wlock.unlock();
    std::shared_lock<std::shared_mutex> rlock(m_mutex);
    for(const auto& it : m_cbs){
        it.second(m_json_data);
    }
    return 0;
}


int ConfigData::setConfig(const nlohmann::json& json_data) {
    std::unique_lock<std::shared_mutex> wlock(m_mutex);
    m_json_data = json_data;
    std::shared_lock<std::shared_mutex> rlock(m_mutex);
    for(const auto& it : m_cbs){
        it.second(m_json_data);
    }
    return 0;
}




int ConfigData::addListener(int key, ConfigData::on_change_cb cb) {
    std::unique_lock<std::shared_mutex> wlock(m_mutex);
    if(key == -1){
        key = ++m_max_listener_id;
    }
    m_cbs[key] = cb;
    return key;
}

int ConfigData::delListener(int key) {
    std::unique_lock<std::shared_mutex> wlock(m_mutex);
    if(m_cbs.find(key) == m_cbs.end() || key <= -1){
        return -1;
    }
    m_cbs.erase(key);
    return 0;
}


int ConfigManager::LoadJson(const std::string& path, int config_id) {
    return LoadJson(path.c_str());
}

int ConfigManager::LoadJson(const char* path, int config_id) {
    std::ifstream file(path);
    if(!file.is_open()) {
        std::cerr << "Failed to open config file: " << path << std::endl;
        return -1;
    }

    nlohmann::json json_data;
    try {
        file >> json_data;
    } catch (const nlohmann::json::parse_error& e) {
        // 日志错误
        return -1;
    }
    getConfigData(config_id)->setConfig(std::move(json_data));
    return 0;
}


}