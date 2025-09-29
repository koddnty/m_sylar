#pragma once

#include <boost/lexical_cast.hpp>
#include <iostream>
#include <yaml-cpp/yaml.h>
#include "log.h"
#include "singleton.h"
#include "until.h"
#include <map>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <list>
#include <functional>
#include <mutex>
#include <shared_mutex>

namespace m_sylar{

class ConfigVarBase{
public:
    using ptr = std::shared_ptr<ConfigVarBase>;
    ConfigVarBase(const std::string& name, const std::string& m_description)
        : m_name(name), m_description(m_description){}

    virtual ~ConfigVarBase() {}

    const std::string& getName () const {return m_name;}
    const std::string& getDescription () const {return m_description;}

    virtual std::string toString() = 0;
    virtual bool fromString(const std::string& val) = 0;
protected:
    std::string m_name;
    std::string m_description;

};

// 模板函数用来做数据的类型转换
template<typename F, typename T>
class LexicalCast{
public:
    T operator() (const F& v){
        return boost::lexical_cast<T>(v);
    }
};

//vec的偏特化
template<typename T>
class LexicalCast<std::string, std::vector<T>>{
public:
    std::vector<T> operator() (const std::string& v){
        YAML::Node node =  YAML::Load(v);
        std::vector<T> vec; 
        for(const auto& it : node){
            vec.push_back(LexicalCast<std::string , T>() (it.Scalar()));
        }
        return vec;
    }
};
template<typename T>
class LexicalCast<std::vector<T>, std::string>{
public:
    std::string operator() (const std::vector<T>& v){
        YAML::Node node;
        for(auto& it : v){
            node.push_back(YAML::Node (LexicalCast<T, std::string>()(it)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

//list的偏特化
template<typename T>
class LexicalCast<std::string, std::list<T>>{
public:
    std::list<T> operator() (const std::string& v){
        YAML::Node node =  YAML::Load(v);
        std::list<T> vec; 
        for(const auto& it : node){
            vec.push_back(LexicalCast<std::string , T>() (it.Scalar()));
        }
        return vec;
    }
};
template<typename T>
class LexicalCast<std::list<T>, std::string>{
public:
    std::string operator() (const std::list<T>& v){
        YAML::Node node;
        for(auto& it : v){
            node.push_back(YAML::Node (LexicalCast<T, std::string>()(it)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

//set的偏特化
template<typename T>
class LexicalCast<std::string, std::set<T>>{
public:
    std::set<T> operator() (const std::string& v){
        YAML::Node node =  YAML::Load(v);
        std::set<T> vec; 
        for(const auto& it : node){
            vec.insert(LexicalCast<std::string , T>() (it.Scalar()));
        }
        return vec;
    }
};
template<typename T>
class LexicalCast<std::set<T>, std::string>{
public:
    std::string operator() (const std::set<T>& v){
        YAML::Node node;
        for(auto& it : v){
            node.push_back(YAML::Node (LexicalCast<T, std::string>()(it)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

//unordered_set的偏特化
template<typename T>
class LexicalCast<std::string, std::unordered_set<T>>{
public:
    std::unordered_set<T> operator() (const std::string& v){
        YAML::Node node =  YAML::Load(v);
        std::unordered_set<T> vec; 
        for(const auto& it : node){
            vec.insert(LexicalCast<std::string , T>() (it.Scalar()));
        }
        return vec;
    }
};
template<typename T>
class LexicalCast<std::unordered_set<T>, std::string>{
public:
    std::string operator() (const std::unordered_set<T>& v){
        YAML::Node node;
        for(auto& it : v){
            node.push_back(YAML::Node (LexicalCast<T, std::string>()(it)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

//map的偏特化
template< typename T>
class LexicalCast<std::string, std::map<std::string, T>>{
public:
    std::map<std::string, T> operator() (const std::string& v){
        YAML::Node node =  YAML::Load(v);
        std::map<std::string, T> map; 
        for(const auto& it : node){
            map.insert(std::make_pair (it.first.Scalar(), LexicalCast<std::string, T>()(it.second.Scalar())));
        }
        return map;
    }
};
template<typename T>
class LexicalCast<std::map<std::string, T>, std::string>{
public:
    std::string operator() (const std::map<std::string, T> & v){
        YAML::Node node;
        for(auto& it : v){
            node[it.first] = YAML::Node (LexicalCast<T, std::string>()(it.second));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

//unordered_map的偏特化
template< typename T>
class LexicalCast<std::string, std::unordered_map<std::string, T>>{
public:
    std::unordered_map<std::string, T> operator() (const std::string& v){
        YAML::Node node =  YAML::Load(v);
        std::unordered_map<std::string, T> map; 
        for(const auto& it : node){
            map.insert(std::make_pair (it.first.Scalar(), LexicalCast<std::string, T>()(it.second.Scalar())));
        }
        return map;
    }
};
template<typename T>
class LexicalCast<std::unordered_map<std::string, T>, std::string>{
public:
    std::string operator() (const std::unordered_map<std::string, T> & v){
        YAML::Node node;
        for(auto& it : v){
            node[it.first] = YAML::Node (LexicalCast<T, std::string>()(it.second));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};




//一个配置量
template<typename T , class FromStr = LexicalCast<std::string, T> 
        , class ToStr = LexicalCast<T, std::string> >
class ConfigVar : public ConfigVarBase {
public:
    using ptr = std::shared_ptr<ConfigVar>;
    using on_change_cb = std::function<void (const T& old_val, const T& new_val)>;
    
    ConfigVar (const std::string& name, const std::string& description, const T& default_value)
            : ConfigVarBase(name, description), m_val(default_value){
    }
            
    std::string toString() override {
        // std::shared_lock<std::shared_mutex> r_lock (m_rwMutex);
        try{
            // return boost::lexical_cast<std::string> (m_val);
            return ToStr()(m_val);
        } catch (std::exception& e){
            M_SYLAR_LOG_ERROR(M_SYLAR_GET_LOGGER_ROOT()) << "ConfigVar::toString"
                << e.what() << "convert : from " << typeid(m_val).name() << "to string";
        }
        return "";
    }
    bool fromString(const std::string& val) override{
        try{
            // m_val = boost::lexical_cast<T> (val);
            setValue(FromStr() (val));
        } catch (std::exception& e){
            M_SYLAR_LOG_ERROR(M_SYLAR_GET_LOGGER_ROOT()) << "ConfigVar::toString"
                << e.what() << "convert : from string to" << typeid(m_val).name();
        }
        return false;
    }
    
    const T getValue () {
        std::shared_lock<std::shared_mutex> r_lock (m_rwMutex);
        return m_val;
    }
    const T setValue (const T& v) {
        std::unique_lock<std::shared_mutex> w_lock (m_rwMutex);
        if(v == m_val){
            return v;
        }
        for(const auto& f_it : m_cbs){
            f_it.second(m_val, v);
        }
        m_val = v; 
        return v;
    }

    //添加监听函数
    void addListener(uint64_t key, on_change_cb cb){
        std::unique_lock<std::shared_mutex> w_lock (m_rwMutex);
        m_cbs[key] = cb;
    }
    //删除监听函数
    void delListener(uint64_t key){
        std::unique_lock<std::shared_mutex> w_lock (m_rwMutex);
        m_cbs.erase(key);
    }
    //获取监听函数
    on_change_cb getListener(uint64_t key){
        std::shared_lock<std::shared_mutex> r_lock (m_rwMutex);
        const auto& it = m_cbs.find(key);
        if(it == m_cbs.end()){
            return nullptr;
        }
        else{
            return it.second;
        }
    }

private:
    T m_val;
    // 变更回调函数 key = uint64_t(唯一hash) on_change_cb回调函数 
    std::map<uint64_t, on_change_cb> m_cbs;
    std::shared_mutex m_rwMutex;
};

class configManager{
public:
    using ptr = std::shared_ptr<configManager>;
    using configValMap = std::map<std::string, std::shared_ptr<ConfigVarBase>>;

    //获取当前名字为name的configVar
    template<class T>
    static typename ConfigVar<T>::ptr Lookup(const std::string& name){
        std::shared_lock<std::shared_mutex> r_lock (get_config_rwMutex());  // //此处不能加锁，涉及递归获取锁问题
        auto it = Get_Datas().find(name);
        if(it == Get_Datas().end()){
            return nullptr;
        }
        else{
            return std::dynamic_pointer_cast<ConfigVar<T> > (it->second);
        }
    }
    
    // 添加新的configVar
    template<class T>
    static typename ConfigVar<T>::ptr Lookup(const std::string& name
                    , const T& defaultValue, const std::string& description){
        {
            std::shared_lock<std::shared_mutex> r_lock (get_config_rwMutex());
            auto it = Get_Datas().find(name);
            if(it != Get_Datas().end()){
                auto temp = std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
                if(temp){
                    M_SYLAR_LOG_INFO(M_SYLAR_GET_LOGGER_ROOT()) << "Lookup name=" << name << " exists";
                    return temp;
                }
                else{
                    M_SYLAR_LOG_ERROR(M_SYLAR_GET_LOGGER_ROOT()) << "Lookup name=" << name << "exists but with an unconvertible type";
                    return temp;
                }
            }
            auto tempVal = Lookup<T>(name);
            if(tempVal){
                M_SYLAR_LOG_INFO(M_SYLAR_GET_LOGGER_ROOT()) << "Lookup name=" << name;
                return tempVal;
            }
        }
        {
            std::unique_lock<std::shared_mutex> w_lock (get_config_rwMutex());
            if(std::string::npos != name.find_first_not_of("qwertyuiopasdfghjklzxcvbnm1234567890._")){
                M_SYLAR_LOG_ERROR(M_SYLAR_GET_LOGGER_ROOT()) << "Lookup name invalid " << name;
                throw std::invalid_argument(name);
            }
            typename ConfigVar<T>::ptr v(new ConfigVar<T>(name, description, defaultValue));
            Get_Datas()[name] = v;
            return v;
        }
    }
    static void LoadFromYaml(const YAML::Node& root);

    static ConfigVarBase::ptr LookupBase(const std::string& name);
    static void Print_all_conf(){
        std::shared_lock<std::shared_mutex> r_lock (get_config_rwMutex());
        for(auto& it : Get_Datas()){
            std::cout << it.first << ">>>" << (it.second)->toString() << std::endl;
        }        
    }
private:
    // 使用静态函数返回静态成员变量，避免静态变量初始化顺序问题
    static configValMap& Get_Datas(){
        static configValMap s_datas;
        return s_datas;
    }
    static std::shared_mutex& get_config_rwMutex () {
        // std::cout << "config_rwMutex 初始化完成" << std::endl;
        static std::shared_mutex config_rwMutex;
        return config_rwMutex;
    }
};



}

