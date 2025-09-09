#include "config.h"

namespace m_sylar{
configManager::configValMap configManager::s_datas;


ConfigVarBase::ptr configManager::LookupBase(const std::string& name){
    auto it = s_datas.find(name);
    return it == s_datas.end() ? nullptr : it->second;
}

static void ListAllMember(const std::string& prefix, const YAML::Node& node,
        std::list<std::pair<std::string , const YAML::Node> >& output){
    if(std::string::npos != prefix.find_first_not_of("qwertyuiopasdfghjklzxcvbnm._0123456789")){
        M_SYLAR_LOG_ERROR(M_SYLAR_GET_LOGGER_ROOT()) << "Config invalid prefix" << prefix << ":" << node; 
        return ;
    }
    std::cout << prefix << " <==> "<< node << std::endl;
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

void configManager::LoadFromYaml(const YAML::Node& root){
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