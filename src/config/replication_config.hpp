#pragma once
#include <string>
#include <variant>
#include <iostream>

// Master Server Configuration
class MasterConfig {
public:
    MasterConfig() = default;
    
    // Master-specific properties
    std::string getReplicationId() const { return replication_id; }
    long long getReplicationOffset() const { return replication_offset; }
    int getReplicaCount() const { return replica_count;}
    void setReplicationId(int id) { replication_id = id; }
    void setReplicationOffset(long long offset) { replication_offset = offset; }

    void incrementReplicaCount() { replica_count++; }
    void decrementReplicaCount() { replica_count--; }
    
    void printInfo() const;
    std::string getInfo() const;


private:
    std::string replication_id = "8371b4fb1155b71f4a04d3e1bc3e18c4a990aeeb";
    long long replication_offset = 0;
    int replica_count=0;
};

// Slave Server Configuration
class SlaveConfig {
public:
    SlaveConfig() = default;
    SlaveConfig(const std::string& host, int port);
    
    // Slave-specific properties
    std::string getMasterHost() const { return master_host; }
    int getMasterPort() const { return master_port; }
    long long getReplicationOffset() const { return replication_offset; }
    
    void setMasterHost(const std::string& host) { master_host = host; }
    void setMasterPort(int port) { master_port = port; }
    void setReplicationOffset(long long offset) { replication_offset = offset; }
    void incrementReplicationOffset(long long bytes) { replication_offset += bytes; }
    
    void printInfo() const;
    std::string getInfo() const;

private:
    std::string master_host = "";
    int master_port = 0;
    long long replication_offset = 0;
};

// Variant-based replication configuration
using ReplicationConfig = std::variant<MasterConfig, SlaveConfig>;

// Helper functions for working with ReplicationConfig variant
namespace ReplicationUtils {
    inline bool isReplica(const ReplicationConfig& config) {
        return std::holds_alternative<SlaveConfig>(config);
    }
    
    
    inline std::string getRole(const ReplicationConfig& config) {

        return isReplica(config) ? "slave" : "master";
    }
    inline std::string getInfo(const ReplicationConfig& config) {
        // std::string rep_info="";
        if (std::holds_alternative<MasterConfig>(config)) {
            return std::get<MasterConfig>(config).getInfo();
        } else {
            return std::get<SlaveConfig>(config).getInfo();
        }
    }
    
    inline void printInfo(const ReplicationConfig& config) {
        if (std::holds_alternative<MasterConfig>(config)) {
            std::get<MasterConfig>(config).printInfo();
        } else {
            std::get<SlaveConfig>(config).printInfo();
        }
    }
}
