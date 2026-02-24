#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <optional>
#include <sys/poll.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include "rdstore.hpp"
#include "resp.hpp"

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

struct ReplicationConfig {
    bool is_replica = false;
    std::string master_host="";
    int master_port = 0;
};

struct ServerConfig {
    int port = 6379;
    // version
    int version = 1;
    // clients
    int client_connected=0;
    // Memory
    int used_memory = 0;

    //Replication
    

    std::string role = "master"; // "master" or "slave"
    struct ReplicationConfig replication_config;
    // std::string master_replid = "";
    // long long master_repl_offset = 0;
};

class RedisServer{
    public:
        RedisServer(int port);
        RedisServer(int port , struct ReplicationConfig rep_config);
        void run();
        void stop();
    private:
        struct ServerConfig config;
        RDStore rd_store;
        int server_fd;

        int setup_server_socket();
        void handle_new_connection(std::vector<struct pollfd> &fds);
        int handle_client_data(int client_fd);
        std::optional<std::string> HandleCommand(int bytes , char buffer[]);
        std::string get_info(ServerInfo flag);

};