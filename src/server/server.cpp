#include "server.hpp"

//////helpers
int set_nonblocking(int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}


// RedisServer///
RedisServer::RedisServer(int port )
    : config(port), command_handler(std::make_unique<CommandHandler>(config)) {}

RedisServer::RedisServer(int port, const ReplicationConfig& rep_config)
    : config(port, rep_config), command_handler(std::make_unique<CommandHandler>(config)) {}

void RedisServer::run(){
    if(config.isReplica()){
        replication_manager = std::make_unique<ReplicationManager>(config.getReplicationConfig(), config.getPort());
        if (!replication_manager->start_handshake()) {
            std::cerr << "master handshake exception";
            return;
        }
    }

    if(setup_server_socket()) return;
    std::cout << "Server started on port " << config.getPort() << " as " << config.getRole() << std::endl;
    std::vector<struct pollfd> fds;
    fds.push_back({server_fd, POLLIN,0});

    while(true){
        int activity = poll(fds.data(),fds.size(),-1);
        if(activity<0){ std::cerr<<"poll failed\n"; return; }
        else if(!activity) continue;
        for(size_t i=0;i<fds.size();i++){
            if(fds[i].revents & POLLIN){
                if(fds[i].fd == server_fd){
                   handle_new_connection(fds);
                }
                else{
                    int ret = handle_client_request(fds[i].fd);
                    if(ret == -1){
                        close(fds[i].fd);
                        config.decrementConnectedClients();
                        fds.erase(fds.begin() + i);
                        i--;
                    }
                }
            }
            
        }
    }

}

void RedisServer::stop(){
    close(server_fd);
}

void RedisServer::handle_new_connection(std::vector<struct pollfd> &fds){
    while(true){
        struct sockaddr_in client_addr;
        int client_addr_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
        if(client_fd < 0){
        std::cerr << "Failed to accept client connection\n";
        break;
        }
        set_nonblocking(client_fd);
        fds.push_back({client_fd, POLLIN, 0});
        config.incrementConnectedClients();
        std::cout << "Client connected : " << client_fd << "\n";
    }
}
int RedisServer::handle_client_request(int client_fd){
    char buffer[1024];
    int bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0) {
        auto ret = command_handler->handleCommand(bytes, buffer);
        if (ret.has_value()) {

            std::string response = *ret;
            // std::cout<<"response : "<<response<<"\n";
            send(client_fd, response.c_str(), response.length(), 0);
            return 0;
        }
        // If command handling failed, close (optional, depending on your logic)
    return -1;
        } else if (bytes == 0) {
        // Connection closed by client
        return -1;
    } else {
        // Error: check if it's just no data available
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;  // No data, but keep connection open
        } else {
            // Other error, close connection
            return -1;
        }
    }


}

std::string RedisServer::get_info(ServerInfo flag){
    
    std::string info_body;
    if(flag & ServerInfo::SERVER){

        info_body = info_body + "redis_version::" + std::to_string(config.getVersion())+ "\r\n";
    }
    if(flag & ServerInfo::CLIENTS){

        info_body = info_body + "connected_clients:" + std::to_string(config.getClientConnected())+ "\r\n";
    }
    if(flag & ServerInfo::MEMORY){

        info_body = info_body + "used_memory:" + std::to_string(config.getUsedMemory())+ "\r\n";
    }
    if(flag & ServerInfo::REPLICATION){

        // info_body = info_body + "role:" + config.getRole() + "\r\n";
        info_body += config.getRepInfo();

    }
    
    // TODO: will add other info later on 
    return resp_parser::encode(resp_value::make_bulk_string(info_body));
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