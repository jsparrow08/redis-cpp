#include "rdstore.hpp"
#include "../config/config.hpp"

size_t RDStore::calculateMemorySize(const std::string& key, const std::string& val) const {
    // Memory calculation: key size + value size + overhead (metadata)
    // Overhead includes: unordered_map node size (estimated ~56 bytes), expiry entry (~24 bytes)
    const size_t overhead_per_entry = 80;  // Conservative estimate
    return key.size() + val.size() + overhead_per_entry;
}

void RDStore::updateMemoryUsage(int delta) {
    total_memory_used += delta;
    if (config_) {
        config_->incrementUsedMemory(delta);
    }
}

bool RDStore::set(std::string &key, std::string &val ,struct set_param *param){
    std::unique_lock lock(mt);
    
    // If key already exists, account for the old value's memory
    auto it = rd_map.find(key);
    if (it != rd_map.end()) {
        size_t old_size = calculateMemorySize(key, it->second.val);
        updateMemoryUsage(-old_size);
    }
    
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
    

    size_t new_size = calculateMemorySize(key, val);
    updateMemoryUsage(new_size);
    
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
            // Key has expired - release memory
            // Need to upgrade to unique lock for modification
            lock.unlock();
            std::unique_lock ulock(mt);
            
            auto it_check = rd_map.find(key);
            auto eit_check = expiry_map.find(key);
            
            if (it_check != rd_map.end() && eit_check != expiry_map.end()) {
                size_t freed_size = calculateMemorySize(key, it_check->second.val);
                rd_map.erase(it_check);
                expiry_map.erase(eit_check);
                updateMemoryUsage(-freed_size);
            }
        }
    }
    return std::nullopt;
}

bool RDStore::del(std::string &key) {
    std::unique_lock lock(mt);
    auto it = rd_map.find(key);
    
    if (it != rd_map.end()) {
        // Calculate and free memory
        size_t freed_size = calculateMemorySize(key, it->second.val);
        
        rd_map.erase(it);
        expiry_map.erase(key);
        
        updateMemoryUsage(-freed_size);
        return true;
    }
    
    return false;
}
