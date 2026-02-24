#include "server.hpp"

//////helpers
int set_nonblocking(int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}


// RedisServer///
RedisServer::RedisServer(int port ){
    config.port = port;
}

RedisServer::RedisServer(int port , struct ReplicationConfig rep_config){
    config.port = port;
    config.replication_config = rep_config;
    if(rep_config.is_replica) config.role = "slave";
    else config.role = "master";
}

void RedisServer::run(){
    if(setup_server_socket()) return;
    std::cout << "Server started on port " << config.port << " as " << config.role << std::endl;
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
                    int ret = handle_client_data(fds[i].fd);
                    if(ret == -1){
                        close(fds[i].fd);
                        config.client_connected=std::max(0,config.client_connected-1);
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
        config.client_connected++;
        std::cout << "Client connected : " << client_fd << "\n";
    }
}
int RedisServer::handle_client_data(int client_fd){
    char buffer[1024];
    int bytes = recv(client_fd,buffer,sizeof(buffer)-1,0);
    if(bytes >0){
        auto ret = HandleCommand(bytes,buffer);
        if(ret.has_value()){
            std::string response = *ret;
            send(client_fd, response.c_str(), response.length(), 0);
            return 0;
        }

    }
    std::cout << "Closing client FD " << client_fd << "\n";
    return -1;


}
std::optional<std::string> RedisServer::HandleCommand(int bytes , char buffer[]){

    std::string input(buffer, bytes);
    struct resp_value  res= resp_parser::decode(input)->first;
    if( res.type == RespType::ARRAY){
      auto arr= std::get<std::vector<resp_value>>(res.data);
      if(arr[0].type != RespType::SIMPLE_STRING ){ return std::nullopt;}
      std::string response;
      std::string cmd=std::get<std::string>(arr[0].data);
      
      if(cmd == "PING"){
        response = resp_parser::encode(resp_value::make_string("PONG"));
      }
      else if(cmd == "ECHO"){
        std::string echo_str = std::get<std::string>(arr[1].data);
        response = resp_parser::encode(resp_value::make_bulk_string(echo_str));
      }
      else if(cmd == "SET"){
        if(arr.size() < 3) return std::nullopt;
        auto st_param_opt = get_set_params(arr);
        bool ret ;
        if(st_param_opt.has_value()) {
          ret = rd_store.set(std::get<std::string>(arr[1].data), 
                                  std::get<std::string>(arr[2].data), 
                                  &st_param_opt.value());
        }
        else {
          ret = rd_store.set(std::get<std::string>(arr[1].data), 
                                  std::get<std::string>(arr[2].data), nullptr);
        }

        // bool ret = set(std::get<std::string>(arr[1].data),std::get<std::string>(arr[2].data),*st_param);
        if(ret)
          response = resp_parser::encode(resp_value::make_string("OK"));
        else 
          response = resp_parser::encode(resp_value::make_null());

      }
      else if(cmd== "GET"){
        if(arr.size()<2) return std::nullopt;
        auto val = rd_store.get(std::get<std::string>(arr[1].data));
        if(!val.has_value())
          response = resp_parser::encode(resp_value::make_null());
        else 
          response = resp_parser::encode(resp_value::make_bulk_string(*val));

      }
      else if(cmd=="INFO"){
        if(arr.size() > 1){
            
            if(std::get<std::string>(arr[1].data)== "replication"){
                response = get_info(ServerInfo::REPLICATION);
                
            }
            else if(std::get<std::string>(arr[1].data)== "clients"){
                response = get_info( ServerInfo::CLIENTS);
            }

        }
        else {
            response = get_info(ServerInfo::ALL);
            
        }
      }
      else{
        // std::cout << "Closing client FD " << client_fd << "\n";
        return std::nullopt;
      }
      return response;

    }
    else 
        return std::nullopt;

}
std::string RedisServer::get_info(ServerInfo flag){
    
    std::string info_body;
    if(flag & ServerInfo::SERVER){
        // info_body += "# Server\r\n";
        info_body = info_body + "redis_version::" + std::to_string(config.version)+ "\r\n";
    }
    if(flag & ServerInfo::CLIENTS){
        // info_body += "# Clients \r\n";
        info_body = info_body + "connected_clients:" + std::to_string(config.client_connected)+ "\r\n";
    }
    if(flag & ServerInfo::MEMORY){
        // info_body += "# Memory \r\n";
        info_body = info_body + "used_memory:" + std::to_string(config.used_memory)+ "\r\n";
    }
    if(flag & ServerInfo::REPLICATION){
        // info_body += "# Replication \r\n";
        info_body = info_body + "role:" + config.role + "\r\n";
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
    server_addr.sin_port = htons(config.port);
    if (bind(serverfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
        std::cerr << "Failed to bind to port "<<config.port<<"\n";
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