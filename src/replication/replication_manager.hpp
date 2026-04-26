#pragma once

#include <memory>
#include <string>
#include "../config/replication_config.hpp"

class ReplicationManager {
public:
    explicit ReplicationManager(const ReplicationConfig& replication_config, int replica_port);
    // explicit ReplicationManager(const ReplicationConfig& replication_config);
    ~ReplicationManager();

    // Start the replica handshake process.
    bool start_handshake();

    

private:
    bool connectToMaster();
    bool send_to_master(std::string str);
    // Send the initial PING to the master.
    bool sendPing();
    bool sendReplconfListeningPort();
    bool sendReplconfCapa();
    // Read and validate the master's PONG reply.
    bool receivePong();
    bool receiveOk();
    //psync commands
    bool sendPsync();
    bool receiveFullresync(); 


    ReplicationConfig replication_config_;
    int master_fd_ = -1;
    int replica_port_;
};
