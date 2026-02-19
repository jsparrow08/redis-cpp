#include "rdstore.hpp"

bool RDStore::set(std::string &key, std::string &val ){
    std::unique_lock lock(mt);
    rd_map[key] = RDObj{val, ValType::STRING};
    return true;
     
}

std::optional<std::string> RDStore::get(std::string &key){
    std::shared_lock lock(mt);
    auto it = rd_map.find(key);
    if(it != rd_map.end() && it->second.val_type == ValType::STRING){
        return it->second.val;
    }
    return std::nullopt;
}

