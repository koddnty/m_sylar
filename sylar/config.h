#pragma once

#include <boost/lexical_cast.hpp>
#include <iostream>
#include <yaml-cpp/yaml.h>
#include "log.h"
#include "singleton.h"
#include "config.h"
#include "until.h"
#include <map>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <list>

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







template<typename T , class FromStr = LexicalCast<std::string, T> 
        , class ToStr = LexicalCast<T, std::string> >
class ConfigVar : public ConfigVarBase {
public:
    using ptr = std::shared_ptr<ConfigVar>;
    
    ConfigVar (const std::string& name, const std::string& description, const T& default_value)
            : ConfigVarBase(name, description), m_val(default_value){

    }
            
    std::string toString() override {
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
    
    const T getValue () const {return m_val;}
    const T setValue (const T& v) {return m_val = v; }
private:
    T m_val;
};

class configManager{
public:
    using ptr = std::shared_ptr<configManager>;
    using configValMap = std::map<std::string, std::shared_ptr<ConfigVarBase>>;

    template<class T>
    static typename ConfigVar<T>::ptr Lookup(const std::string& name){
        auto it = s_datas.find(name);
        if(it == s_datas.end()){
            return nullptr;
        }
        else{
            return std::dynamic_pointer_cast<ConfigVar<T> > (it->second);
        }
    }

    template<class T>
    static typename ConfigVar<T>::ptr Lookup(const std::string& name
                    , const T& defaultValue, const std::string& description){
        auto tempVal = Lookup<T>(name);
        if(tempVal){
            M_SYLAR_LOG_INFO(M_SYLAR_GET_LOGGER_ROOT()) << "Lookup name=" << name;
            return tempVal;
        }
        if(std::string::npos != name.find_first_not_of("qwertyuiopasdfghjklzxcvbnm1234567890._")){
            M_SYLAR_LOG_ERROR(M_SYLAR_GET_LOGGER_ROOT()) << "Lookup name invalid " << name;
            throw std::invalid_argument(name);
        }
        typename ConfigVar<T>::ptr v(new ConfigVar<T>(name, description, defaultValue));
        s_datas[name] = v;
        return v;
    }
    static void LoadFromYaml(const YAML::Node& root);

    static ConfigVarBase::ptr LookupBase(const std::string& name);
private:
    static configValMap s_datas;
};



}

