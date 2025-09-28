#include "config.h"

namespace m_sylar{
configManager::configValMap Get_Datas();


ConfigVarBase::ptr configManager::LookupBase(const std::string& name){
    auto it = Get_Datas().find(name);
    return it == Get_Datas().end() ? nullptr : it->second;
}

// 获取yaml所有节点输入到output
static void ListAllMember(const std::string& prefix, const YAML::Node& node,
    std::list<std::pair<std::string , const YAML::Node> >& output){
    if(std::string::npos != prefix.find_first_not_of("qwertyuiopasdfghjklzxcvbnm._0123456789")){
        M_SYLAR_LOG_ERROR(M_SYLAR_GET_LOGGER_ROOT()) << "Config invalid prefix" << prefix << ":" << node; 
        return ;
    }
    output.push_back({prefix, node});
    if(node.IsMap()){
        for(auto& it : node){
            ListAllMember(prefix.empty() ? it.first.Scalar() : prefix + "." + it.first.Scalar() , it.second, output);
        }
    }
    else if(node.IsSequence()){
        for(size_t i = 0; i < node.size(); i++){
            if(node[i].IsMap()){
                ListAllMember(prefix.empty() ? std::to_string(i) : prefix + "." + std::to_string(i) , node[i], output);
            }
        }
    }
}

//将每个节点创建为conf并注册到confManager
void configManager::LoadFromYaml(const YAML::Node& root){
    std::unique_lock<std::shared_mutex> w_lock (get_config_rwMutex());
    
    std::list<std::pair<std::string, const YAML::Node>> allNodes;
    ListAllMember("", root, allNodes);
    for(auto& it : allNodes){
        std::string key = it.first;
        if(key.empty()){
            continue;
        } 
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        ConfigVarBase::ptr var = LookupBase(key);
        if(var){
            if(it.second.IsScalar()){
                var->fromString(it.second.Scalar());
            }
            else{
                std::stringstream ss;
                ss << it.second;
                var->fromString(ss.str());
            }
        }
    }
} 


}