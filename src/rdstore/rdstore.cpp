#include "rdstore.hpp"

bool RDStore::set(std::string &key, std::string &val ,struct set_param *param){
    std::unique_lock lock(mt);
    long long expiry_at=-1; 
    if(param){  
        long long now_ms = get_current_time_ms();
        if(param->flag == SetFlag::EX)
            expiry_at = now_ms + (long long) param->expiry *1000;
        if (param->flag == SetFlag::PX)
            expiry_at = now_ms + (long long) param->expiry;
    }
    rd_map[key] = RDObj{val, ValType::STRING};
    expiry_map[key] = expiry_at;
    // will implement active expiry later using heap (priority queue)
    return true;
     
}

std::optional<std::string> RDStore::get(std::string &key){
    std::shared_lock lock(mt);
    auto it = rd_map.find(key);
    auto eit = expiry_map.find(key);

    
    if(eit != expiry_map.end() && it != rd_map.end() && it->second.val_type == ValType::STRING){
        if(eit->second == -1 || eit->second > get_current_time_ms()) 
            return it->second.val;
        else {
            rd_map.erase(it);
            expiry_map.erase(eit);
        }
    }
    return std::nullopt;
}
