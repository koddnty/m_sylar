#pragma once

#include <boost/lexical_cast.hpp>
#include "log.h"
#include "singleton.h"
#include "config.h"
#include <iostream>
#include "until.h"


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

template<class T >
class ConfigVar : public ConfigVarBase {
public:
    using ptr = std::shared_ptr<ConfigVar>;
    
    ConfigVar (const std::string& name, const std::string& description, const T& default_value)
            : ConfigVarBase(name, description), m_val(default_value){}
    std::string toString() override {
        try{
            return boost::lexical_cast<std::string> (m_val);
        } catch (std::exception& e){
            M_SYLAR_LOG_ERROR(M_SYLAR_GET_LOGGER_ROOT()) << "ConfigVar::toString"
                << e.what() << "convert : from " << typeid(m_val).name() << "to string";
        }
        return "";
    }
    bool fromString(const std::string& val) override{
        try{
            m_val = boost::lexical_cast<T> (val);
        } catch (std::exception& e){
            {M_SYLAR_LOG_ERROR(M_SYLAR_GET_LOGGER_ROOT()) << "ConfigVar::toString"
                << e.what() << "convert : from string to" << typeid(m_val).name();}
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
        if(std::string::npos != name.find_first_not_of("qwertyuiopasdfghjklzxcvbnm1234567890._QWERTYUIOPASDFGHJKLZXCVBNM")){
            M_SYLAR_LOG_ERROR(M_SYLAR_GET_LOGGER_ROOT()) << "Lookup name invalid " << name;
            throw std::invalid_argument(name);
        }
        typename ConfigVar<T>::ptr v(new ConfigVar<T>(name, description, defaultValue));
        s_datas[name] = v;
        return v;
    }
private:
    static configValMap s_datas;
};



}

