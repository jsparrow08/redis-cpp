#include "server.hpp"
#include <algorithm>
#include <errno.h>

//////helpers
int set_nonblocking(int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}


// RedisServer
RedisServer::RedisServer(int port )
    : config(port), command_handler(std::make_unique<CommandHandler>(config)) {}

RedisServer::RedisServer(int port, const ReplicationConfig& rep_config)
    : config(port, rep_config), command_handler(std::make_unique<CommandHandler>(config)) {}

void RedisServer::run(){
    // If we're a replica, start handshake with master
    if(config.isReplica()){
        setup_replica_mode();
    }

    if(setup_server_socket()) return;
    std::cout << "Server started on port " << config.getPort() << " as " << config.getRole() << std::endl;
    
    // Main event loop
    while(true){
        // Build pollfd array from all active connections
        std::vector<struct pollfd> fds;
        
        // Add server socket (listening for new connections)
        fds.push_back({server_fd, POLLIN, 0});
        
        // Add all active connections
        for(auto& [fd, conn] : connections) {
            short events = 0;
            
            // Always listen for input
            events |= POLLIN;
            
            // Listen for writability if we have data to send
            if(!conn.output_buffer.empty()) {
                events |= POLLOUT;
            }
            
            fds.push_back({fd, events, 0});
        }
        
        // // Add master fd if in replica mode
        // if(master_fd >= 0) {
        //     fds.push_back({master_fd, POLLIN, 0});
        // }
        
        int activity = poll(fds.data(), fds.size(), -1);
        if(activity < 0){ 
            std::cerr << "poll failed: " << strerror(errno) << "\n"; 
            return; 
        }
        else if(!activity) continue;
        
        // Process all ready file descriptors
        for(const auto& pfd : fds) {
            if(pfd.revents == 0) continue;
            
            // New client connection
            if(pfd.fd == server_fd && (pfd.revents & POLLIN)) {
                accept_new_connection();
            }
            // Existing connection is readable
            else if(pfd.revents & POLLIN) {
                process_readable_connection(pfd.fd);
            }
            // Existing connection is writable
            if(pfd.revents & POLLOUT) {
                process_writable_connection(pfd.fd);
            }
        }
    }
}

void RedisServer::stop(){
    close(server_fd);
}

void RedisServer::setup_replica_mode() {
    replication_manager = std::make_unique<ReplicationManager>(config.getReplicationConfig(), config.getPort());
    if (!replication_manager->start_handshake()) {
        std::cerr << "Replica handshake failed\n";
        return;
    }
    
    master_fd = replication_manager->get_master_fd();
    if(master_fd < 0) {
        std::cerr << "Failed to get master fd\n";
        return;
    }
    
    set_nonblocking(master_fd);

    Connection master_conn(master_fd, ConnectionState::COMMAND_STREAMING, CommandSource::REPLICATION);
    master_conn.input_buffer = replication_manager->get_leftover_bytes();
    connections[master_fd] = master_conn;
    
    std::cout << "[REPLICA] Connected to master on fd: " << master_fd << std::endl;

    Connection& conn = connections[master_fd];
    while(!conn.input_buffer.empty()) {
        auto parse_result = try_parse_command(conn);
        if(!parse_result) {
            break; 
        }
        
        auto [parsed_value, bytes_consumed] = *parse_result;
        conn.input_buffer.erase(0, bytes_consumed);
        conn.bytes_processed = 0;
        
        if(parsed_value.type == RespType::ARRAY) {
            auto arr = std::get<std::vector<resp_value>>(parsed_value.data);
            process_replicated_command(master_fd, arr, bytes_consumed);
        }
    }
}

void RedisServer::accept_new_connection(){
    while(true){
        struct sockaddr_in client_addr;
        int client_addr_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
        if(client_fd < 0){
            if(errno == EAGAIN || errno == EWOULDBLOCK) {
                break;  // No more connections
            }
            std::cerr << "Failed to accept client connection\n";
            break;
        }
        set_nonblocking(client_fd);
        
        // Create connection object
        Connection conn(client_fd, ConnectionState::CLIENT_COMMAND_WAITING, CommandSource::CLIENT);
        connections[client_fd] = conn;
        config.incrementConnectedClients();
        std::cout << "[CONNECTION] Client connected on fd: " << client_fd << "\n";
    }
}

void RedisServer::process_readable_connection(int fd) {
    auto it = connections.find(fd);
    if(it == connections.end()) return;
    
    Connection& conn = it->second;
    
    // Read data into input_buffer
    char buffer[4096];
    int bytes = recv(fd, buffer, sizeof(buffer), 0);
    
    if(bytes < 0) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            return;  // No data available
        }
        std::cerr << "[ERROR] recv failed on fd " << fd << ": " << strerror(errno) << "\n";
        handle_connection_close(fd);
        return;
    }
    
    if(bytes == 0) {
        std::cout << "[CONNECTION] Client closed connection on fd: " << fd << "\n";
        handle_connection_close(fd);
        return;
    }
    
    // Append to input buffer
    conn.input_buffer.append(buffer, bytes);
    
    // Try to parse complete commands from input buffer
    while(!conn.input_buffer.empty()) {
        auto parse_result = try_parse_command(conn);
        
        if(!parse_result) {
            // Incomplete command, wait for more data
            break;
        }
        
        auto [parsed_value, bytes_consumed] = *parse_result;
        
        // Remove parsed bytes from buffer
        conn.input_buffer.erase(0, bytes_consumed);
        conn.bytes_processed = 0;
        
        // Handle different connection states
        if(parsed_value.type != RespType::ARRAY) {
            std::cerr << "[ERROR] Expected RESP array, got type: " << (int)parsed_value.type << " on fd: " << fd << "\n";
            // For master connection, skip this malformed data and continue
            // For client connection, close the connection
            if(fd == master_fd) {
                std::cerr << "[ERROR] Malformed data from master, continuing\n";
                continue;
            } else {
                std::cerr << "[ERROR] Malformed data from client, closing\n";
                handle_connection_close(fd);
                return;
            }
        }
        
        auto arr = std::get<std::vector<resp_value>>(parsed_value.data);
        
        // Process command based on connection state
        if(conn.state == ConnectionState::COMMAND_STREAMING) {
            // Replicated command from master - process without sending response
            process_replicated_command(fd, arr, bytes_consumed);
        } 
        else if(conn.state == ConnectionState::CLIENT_COMMAND_WAITING) {
            // Check if this is a write command (for propagation)
            bool is_write_cmd = false;
            if(!arr.empty() && arr[0].type == RespType::BULK_STRING) {
                std::string cmd = std::get<std::string>(arr[0].data);
                std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);
                is_write_cmd = (cmd == "SET" || cmd == "DEL");
            }
            
            // Regular client command
            auto response = command_handler->handleCommand(arr, CommandSource::CLIENT);
            if(response.has_value()) {
                queue_response(fd, *response);
            }
            
            // Propagate write commands to replicas if we're a master
            if(is_write_cmd && !config.isReplica()) {
                propagate_write_command(arr);
            }
            
            // Check if this is PSYNC command (indicates replica connecting)
            if(!arr.empty() && arr[0].type == RespType::BULK_STRING) {
                std::string cmd = std::get<std::string>(arr[0].data);
                std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);
                
                if(cmd == "PSYNC" && !config.isReplica()) {
                    // This is a replica connection - mark it for command propagation
                    conn.is_replica_connection = true;
                    conn.state = ConnectionState::COMMAND_STREAMING;
                    config.incrementReplicaCount();
                    std::cout << "[MASTER] Replica connected on fd: " << fd << "\n";
                }
            }
        }
    }
    
    // Update connection in map
    connections[fd] = conn;
}

void RedisServer::process_writable_connection(int fd) {
    auto it = connections.find(fd);
    if(it == connections.end()) return;
    
    Connection& conn = it->second;
    
    if(conn.output_buffer.empty()) {
        return;  // Nothing to send
    }
    
    int bytes_sent = send(fd, conn.output_buffer.c_str(), conn.output_buffer.length(), 0);
    
    if(bytes_sent < 0) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            return;  // Will retry when writable again
        }
        std::cerr << "[ERROR] send failed on fd " << fd << ": " << strerror(errno) << "\n";
        handle_connection_close(fd);
        return;
    }
    
    // Remove sent bytes from buffer
    conn.output_buffer.erase(0, bytes_sent);
    connections[fd] = conn;
}

void RedisServer::flush_output_buffer(int fd) {
    auto it = connections.find(fd);
    if(it == connections.end()) return;
    
    Connection& conn = it->second;
    
    while(!conn.output_buffer.empty()) {
        int bytes_sent = send(fd, conn.output_buffer.c_str(), conn.output_buffer.length(), 0);
        
        if(bytes_sent < 0) {
            if(errno == EAGAIN || errno == EWOULDBLOCK) {
                break;  // Will retry in next poll
            }
            std::cerr << "[ERROR] flush failed on fd " << fd << "\n";
            handle_connection_close(fd);
            return;
        }
        
        conn.output_buffer.erase(0, bytes_sent);
    }
    
    connections[fd] = conn;
}

void RedisServer::handle_connection_close(int fd) {
    auto it = connections.find(fd);
    if(it != connections.end()) {
        close(fd);
        connections.erase(it);
        
        if(fd != master_fd) {
            config.decrementConnectedClients();
        } else {
            master_fd = -1;
            std::cout << "[REPLICA] Master connection closed\n";
        }
    }
}

void RedisServer::process_replicated_command(int fd, const std::vector<resp_value>& parsed_command, size_t bytes_consumed) {
    std::cout << "[REPLICATION] Processing replicated command\n";
    
    auto response = command_handler->handleCommand(parsed_command, CommandSource::REPLICATION);
    
    // If there's a response (e.g., REPLCONF GETACK), queue it to send back to master
    if(response.has_value()) {
        queue_response(fd, *response);
    }
    
    // Increment the replica offset by the number of bytes consumed
    // This is done AFTER processing the command
    config.incrementReplicaOffset(bytes_consumed);
    
    std::cout << "[REPLICATION] Command processed on replica\n";
}

void RedisServer::propagate_write_command(const std::vector<resp_value>& args) {
    // Encode the command as RESP for propagation
    std::string propagated_cmd = resp_parser::encode(resp_value::make_array(args));
    
    std::cout << "[PROPAGATE] Sending write command to replicas\n";
    
    // Send to all replica connections
    for(auto& [fd, conn] : connections) {
        if(conn.is_replica_connection && conn.state == ConnectionState::COMMAND_STREAMING) {
            queue_response(fd, propagated_cmd);
            std::cout << "[PROPAGATE] Queued command for replica fd: " << fd << "\n";
        }
    }
}

std::optional<std::pair<resp_value, size_t>> RedisServer::try_parse_command(Connection& conn) {
    auto decode_result = resp_parser::decode(conn.input_buffer);
    return decode_result;
}

void RedisServer::queue_response(int fd, const std::string& response) {
    auto it = connections.find(fd);
    if(it != connections.end()) {
        it->second.output_buffer += response;
    }
}

int RedisServer::setup_server_socket(){
    int serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverfd < 0) {
        std::cerr << "Failed to create server socket\n";
        return 1;
    }
    int reuse = 1;
    if (setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        std::cerr << "setsockopt failed\n";
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(config.getPort());
    if (bind(serverfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
        std::cerr << "Failed to bind to port "<<config.getPort()<<"\n";
        return 1;
    }

    if (listen(serverfd, 5) != 0) {
        std::cerr << "listen failed\n";
        return 1;
    }
    set_nonblocking(serverfd);
    server_fd = serverfd;
    return 0;
}