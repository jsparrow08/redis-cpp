#pragma once
#include <string>
#include <vector>
#include <memory>
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
#include <set>
#include "../config/config.hpp"
#include "../command/command_handler.hpp"
#include "../replication/replication_manager.hpp"
#include "../resp/resp.hpp"


class RedisServer{
    public:
        RedisServer(int port);
        RedisServer(int port, const ReplicationConfig& rep_config);
        void run();
        void stop();
    private:
        Config config;
        std::unique_ptr<CommandHandler> command_handler;
        std::unique_ptr<ReplicationManager> replication_manager;
        int server_fd;
        std::set<int> replica_connections;  

        int setup_server_socket();
        void handle_new_connection(std::vector<struct pollfd> &fds);
        int handle_client_request(int client_fd);
        std::string get_info(ServerInfo flag);
        void propagate_command_to_replicas(const std::string& command_resp);
        void register_replica_connection(int client_fd);
        bool is_write_command(const std::string& command_name);
        std::optional<std::string> extract_command_name(const std::string& raw_input);

};