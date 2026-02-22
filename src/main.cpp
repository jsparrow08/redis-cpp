#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <vector>
#include <arpa/inet.h>
#include <netdb.h>
#include <unordered_map>
#include "resp.hpp"
#include "rdstore.hpp"



static RDStore rd_store;
static int port = 6379;

int set_nonblocking(int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

std::optional<set_param> get_set_params(std::vector<resp_value> &arr){
  struct set_param param;
  if(arr.size()<5){
    return std::nullopt;
  }
  if(std::get<std::string>(arr[3].data) == "EX"){
    param = {(long long)(std::stoi(std::get<std::string>(arr[4].data)) ),SetFlag::EX};
  }
  if(std::get<std::string>(arr[3].data) == "PX"){
    param = {(long long)(std::stoi(std::get<std::string>(arr[4].data))),SetFlag::PX};
  }
  return param;
}


int handle_client(int client_fd){
  char buffer[1024];
  int bytes = recv(client_fd,buffer,sizeof(buffer)-1,0);
  if(bytes >0){
    // TODO: parse message
    std::string input(buffer, bytes);
    struct resp_value  res= resp_parser::decode(input)->first;
    if( res.type == RespType::ARRAY){
      auto arr= std::get<std::vector<resp_value>>(res.data);
      if(arr[0].type != RespType::SIMPLE_STRING ){ return -1;}
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
        if(arr.size() < 3) return -1;
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

        // bool ret = rd_store.set(std::get<std::string>(arr[1].data),std::get<std::string>(arr[2].data),*st_param);
        if(ret)
          response = resp_parser::encode(resp_value::make_string("OK"));
        else 
          response = resp_parser::encode(resp_value::make_null());

      }
      else if(cmd== "GET"){
        if(arr.size()<2) return -1;
        auto val = rd_store.get(std::get<std::string>(arr[1].data));
        if(!val.has_value())
          response = resp_parser::encode(resp_value::make_null());
        else 
          response = resp_parser::encode(resp_value::make_bulk_string(*val));

      }
      else{
        std::cout << "Closing client FD " << client_fd << "\n";
        return -1;
      }
      send(client_fd, response.c_str(), response.length(), 0);

    }
    

    return 0;
  }
  else {
    std::cout << "Closing client FD " << client_fd << "\n";
    return -1;
  }


  
  // std::cout << "Response send\n";
}

int main(int argc, char **argv) {

  if(argc>=3 && !std::strcmp(argv[1],"--port"))
      port = std::stoi(argv[2]);

  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
   std::cerr << "Failed to create server socket\n";
   return 1;
  }
  
  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    std::cerr << "setsockopt failed\n";
    return 1;
  }
  
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);
  
  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
    std::cerr << "Failed to bind to port "<<port<<"\n";
    return 1;
  }
  
  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return 1;
  }

  set_nonblocking(server_fd);
  std::vector<struct pollfd> fds;
  fds.push_back({server_fd, POLLIN,0});

  while(true){
    int activity = poll(fds.data(),fds.size(),-1);
    if(activity<0){
      std::cerr<<"poll failed\n";
      return 1;
    }
    else if(activity == 0){
      continue;
    }
    for(size_t i=0;i<fds.size();i++){
      if(fds[i].revents & POLLIN){
        if(fds[i].fd == server_fd){
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
              std::cout << "Client connected : " << client_fd << "\n";
          }
        }
        else{
          int result = handle_client(fds[i].fd);
          if(result == -1){
            close(fds[i].fd);
            fds.erase(fds.begin() + i);
            i--;
          }
        }
      }
    }
      


  }
  
  close(server_fd);

  return 0;
}
