#include <unistd.h>
#include <sys/socket.h>
#include <iostream>
#include <netdb.h>
#include <cstring>
#include <errno.h>

#include "replication_manager.hpp"

ReplicationManager::ReplicationManager(const ReplicationConfig& replication_config)
    : replication_config_(replication_config) {}

ReplicationManager::~ReplicationManager() = default;

bool ReplicationManager::start_handshake() {
    return connectToMaster() && sendPing() && receivePong();
}

bool ReplicationManager::sendPing() {
    // TODO: send RESP PING to master
    std::string ping = "*1\r\n$4\r\nPING\r\n";
    
    return send_to_master(ping);
}

bool ReplicationManager::receivePong() {
    // TODO: read and validate the master's PONG reply
    
    return true;
}

bool ReplicationManager::send_to_master(std::string str){
    int ret = send(master_fd_, str.c_str(), str.length(), 0); 
    if(ret <0) return false;
    return true;
}

bool ReplicationManager::connectToMaster() {

    if (!std::holds_alternative<SlaveConfig>(replication_config_)) {
    std::cerr << "ReplicationManager::connectToMaster() called on non-slave config\n";
    return false;
    }

    const auto& slave_config = std::get<SlaveConfig>(replication_config_);
    std::string master_host = slave_config.getMasterHost();
    int master_port = slave_config.getMasterPort();

    master_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (master_fd_ < 0) {
        perror("Socket creation failed");
        return false;
    }

    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;     
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(master_host.c_str(), std::to_string(master_port).c_str(), &hints, &res);
    if (status != 0) {
        std::cerr << "getaddrinfo failed: " << gai_strerror(status) << "\n";
        close(master_fd_);
        return false;
    }

    if (connect(master_fd_, res->ai_addr, res->ai_addrlen) < 0) {
        perror("Connection failed");
        freeaddrinfo(res);
        close(master_fd_);
        return false;
    }

    freeaddrinfo(res);
    std::cout << "Replica: connected to master " << master_host << ":" << master_port << "\n";
    return true;
}
