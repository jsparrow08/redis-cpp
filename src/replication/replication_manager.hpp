#pragma once

#include <memory>
#include <string>
#include "../config/replication_config.hpp"

class ReplicationManager {
public:
    explicit ReplicationManager(const ReplicationConfig& replication_config, int replica_port);
    ~ReplicationManager();

    // Start the replica handshake process.
    bool start_handshake();
    
    // Get the master file descriptor (after handshake completes)
    int get_master_fd() const { return master_fd_; }
    std::string get_leftover_bytes() const { return leftover_bytes_; }
    
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
    // bool receiveRdbFile();

    ReplicationConfig replication_config_;
    std::string leftover_bytes_;
    int master_fd_ = -1;
    int replica_port_;
};
