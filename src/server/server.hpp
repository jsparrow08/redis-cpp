#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>
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
#include "../config/config.hpp"
#include "../command/command_handler.hpp"
#include "../replication/replication_manager.hpp"
#include "../resp/resp.hpp"
#include "connection.hpp"


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
        int master_fd = -1;  // For replica mode: fd of connection to master
        
        // Map of all active connections (fd -> Connection)
        std::map<int, Connection> connections;

        int setup_server_socket();
        void accept_new_connection();
        void process_readable_connection(int fd);
        void process_writable_connection(int fd);
        void flush_output_buffer(int fd);
        void handle_connection_close(int fd);
        
        // Replication-specific processing
        void setup_replica_mode();
        void process_replicated_command(int fd, const std::vector<resp_value>& parsed_command, size_t bytes_consumed);
        void propagate_write_command(const std::vector<resp_value>& args);
        
        // Helper methods
        std::optional<std::pair<resp_value, size_t>> try_parse_command(Connection& conn);
        void queue_response(int fd, const std::string& response);

};