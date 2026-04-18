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
#include "config/config.hpp"
#include "rdstore.hpp"
#include "resp.hpp"

class RedisServer{
    public:
        RedisServer(int port);
        RedisServer(int port, const ReplicationConfig& rep_config);
        void run();
        void stop();
    private:
        Config config;
        RDStore rd_store;
        int server_fd;

        int setup_server_socket();
        void handle_new_connection(std::vector<struct pollfd> &fds);
        int handle_client_data(int client_fd);
        std::optional<std::string> HandleCommand(int bytes , char buffer[]);
        std::string get_info(ServerInfo flag);

};