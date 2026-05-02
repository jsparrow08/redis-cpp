#pragma once
#include <string>
#include "replication_config.hpp"

enum class ServerInfo : unsigned int{
    SERVER = 1,
    CLIENTS = 2,
    MEMORY = 4,
    REPLICATION = 8,
    ALL = SERVER | CLIENTS | MEMORY | REPLICATION 
};

// Operator overloads for ServerInfo bitwise operations
inline ServerInfo operator|(ServerInfo a, ServerInfo b) {
    return static_cast<ServerInfo>(static_cast<unsigned int>(a) | static_cast<unsigned int>(b));
}

inline bool operator&(ServerInfo a, ServerInfo b) {
    return (static_cast<unsigned int>(a) & static_cast<unsigned int>(b)) != 0;
}

class Config {
public:
    // Constructors
    Config();
    Config(int port);
    Config(int port, const ReplicationConfig& rep_config);

    // Getters
    int getPort() const { return port; }
    int getVersion() const { return version; }
    int getClientConnected() const { return client_connected; }
    int getUsedMemory() const { return used_memory; }
    std::string getRole() const;
    std::string getRepInfo() const;
    const ReplicationConfig& getReplicationConfig() const { return replication_config; }
    bool isReplica() const;

    // Setters
    void setPort(int p) { port = p; }
    void setVersion(int v) { version = v; }
    void setClientConnected(int c) { client_connected = c; }
    void setUsedMemory(int m) { used_memory = m; }
    void setReplicationConfig(const ReplicationConfig& rep_config) { replication_config = rep_config; }

    // Utility methods
    void incrementConnectedClients() { client_connected++; }
    void decrementConnectedClients() { client_connected = std::max(0, client_connected - 1); }
    void incrementUsedMemory(int bytes_used) {used_memory+=bytes_used;}
    
    // Replication offset tracking for replicas
    void incrementReplicaOffset(long long bytes) {
        if (std::holds_alternative<SlaveConfig>(replication_config)) {
            auto& slave_config = std::get<SlaveConfig>(replication_config);
            slave_config.incrementReplicationOffset(bytes);
        }
    }
    
    long long getReplicaOffset() const {
        if (std::holds_alternative<SlaveConfig>(replication_config)) {
            return std::get<SlaveConfig>(replication_config).getReplicationOffset();
        }
        return 0;
    }

private:
    int port = 6379;
    int version = 1;
    int client_connected = 0;
    int used_memory = 0;
    ReplicationConfig replication_config;
};

// Inline implementations
inline Config::Config() : replication_config(MasterConfig()) {}

inline Config::Config(int port) : port(port), replication_config(MasterConfig()) {}

inline Config::Config(int port, const ReplicationConfig& rep_config)
    : port(port), replication_config(rep_config) {}

inline std::string Config::getRepInfo() const { 
    return ReplicationUtils::getInfo(replication_config);
}
inline std::string Config::getRole() const { 
    return ReplicationUtils::getRole(replication_config);
}

inline bool Config::isReplica() const { 
    return ReplicationUtils::isReplica(replication_config);
}
