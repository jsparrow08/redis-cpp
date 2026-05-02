#include <unistd.h>
#include <sys/socket.h>
#include <iostream>
#include <netdb.h>
#include <cstring>
#include <errno.h>

#include "replication_manager.hpp"

// ReplicationManager::ReplicationManager(const ReplicationConfig& replication_config)
//     : replication_config_(replication_config) {}

ReplicationManager::~ReplicationManager() = default;

ReplicationManager::ReplicationManager(const ReplicationConfig& replication_config, int replica_port)
    : replication_config_(replication_config), replica_port_(replica_port) {}

bool ReplicationManager::start_handshake() {
    return connectToMaster() && sendPing() && receivePong() && sendReplconfListeningPort() && sendReplconfCapa() && 
           sendPsync();;
}

bool ReplicationManager::sendPing() {
    // TODO: send RESP PING to master
    std::string ping = "*1\r\n$4\r\nPING\r\n";
    
    return send_to_master(ping);
}

bool ReplicationManager::receivePong() {
    // TODO: read and validate the master's PONG reply
    char buffer[1024];
    int n = recv(master_fd_, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0) {
        std::cerr << "Failed to receive PONG from master\n";
        return false;
    }
    buffer[n] = '\0';  // Null-terminate for string comparison
    std::string response(buffer);
    if (response == "+PONG\r\n") {
        return true;
    }
    std::cerr << "Unexpected PONG response: " << response << "\n";
    return false;
    return true;
}

bool ReplicationManager::receiveOk() {
    char buffer[1024];
    int n = recv(master_fd_, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0) {
        std::cerr << "Failed to receive OK from master\n";
        return false;
    }
    buffer[n] = '\0';
    std::string response(buffer);
    if (response == "+OK\r\n") {
        return true;
    }
    std::cerr << "Unexpected OK response: " << response << "\n";
    return false;
}

bool ReplicationManager::sendReplconfListeningPort() {
    std::string port_str = std::to_string(replica_port_);
    std::string replconf = "*3\r\n$8\r\nREPLCONF\r\n$14\r\nlistening-port\r\n$" +
                           std::to_string(port_str.length()) + "\r\n" + port_str + "\r\n";
    if (!send_to_master(replconf)) {
        return false;
    }
    return receiveOk();
}

bool ReplicationManager::sendReplconfCapa() {
    std::string replconf = "*3\r\n$8\r\nREPLCONF\r\n$4\r\ncapa\r\n$6\r\npsync2\r\n";
    if (!send_to_master(replconf)) {
        return false;
    }
    return receiveOk();
}

bool ReplicationManager::send_to_master(std::string str){
    int ret = send(master_fd_, str.c_str(), str.length(), 0); 
    if(ret <0) return false;
    return true;
}

bool ReplicationManager::sendPsync() {
    // PSYNC ? -1
    // * = 3 elements: PSYNC, ?, -1
    // $5 = length of "PSYNC"
    // $1 = length of "?"
    // $2 = length of "-1"
    std::string psync = "*3\r\n$5\r\nPSYNC\r\n$1\r\n?\r\n$2\r\n-1\r\n";
    if (!send_to_master(psync)) {
        return false;
    }
    return receiveFullresync();
}

bool ReplicationManager::receiveFullresync() {
    std::string response;
    char buffer[4096];
    
    // 1. Read until we see the start of the RDB file ($...) and its trailing \r\n
    while (true) {
        int n = recv(master_fd_, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0) return false;
        
        buffer[n] = '\0';
        response.append(buffer, n);
        
        size_t rdb_idx = response.find("\r\n$");
        if (rdb_idx != std::string::npos) {
            if (response.find("\r\n", rdb_idx + 3) != std::string::npos) {
                break; // We have the length header!
            }
        }
    }
    
    if (response.find("+FULLRESYNC") == 0) {
        std::cout << "Received FULLRESYNC\n";
        
        size_t rdb_idx = response.find("\r\n$");
        size_t length_end = response.find("\r\n", rdb_idx + 3);
        
        // Parse the exact length of the RDB file
        int rdb_length = std::stoi(response.substr(rdb_idx + 3, length_end - (rdb_idx + 3)));
        size_t rdb_data_start = length_end + 2; // Move past the \r\n
        int bytes_in_buffer = response.length() - rdb_data_start;
        
        // 2. Read the remaining binary RDB data if it wasn't fully captured in the first recv()
        int bytes_remaining = rdb_length - bytes_in_buffer;
        while (bytes_remaining > 0) {
            int n = recv(master_fd_, buffer, std::min((int)sizeof(buffer) - 1, bytes_remaining), 0);
            if (n <= 0) break;
            bytes_remaining -= n;
        }
        
        // 3. If we over-read (TCP packet contained RDB + REPLCONF GETACK command)
        // Save it so we can hand it to the event loop!
        if (bytes_in_buffer > rdb_length) {
            leftover_bytes_ = response.substr(rdb_data_start + rdb_length);
        }
        
        return true;
    }
    
    std::cerr << "Unexpected FULLRESYNC response: " << response << "\n";
    return false;
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
