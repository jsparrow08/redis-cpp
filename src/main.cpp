#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <netdb.h>
#include <unordered_map>
#include "server.hpp"





int main(int argc, char **argv) {
  int port = 6379;
  struct ReplicationConfig rep_config;

  for (int i = 1; i < argc; i++){
		if (std::string(argv[i]) == "--port" && i + 1 < argc)
			port = std::atoi(argv[++i]);
		if (std::string(argv[i]) == "--replicaof" && i+1 <argc){
      rep_config.is_replica=true;
      std::string arg(argv[++i]);
      size_t space = arg.find(' ');
      if(space != std::string::npos) {
            rep_config.master_host = arg.substr(0, space);
            rep_config.master_port = stoi(arg.substr(space+1));
        }

    }
	}


  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  
  RedisServer redis_server(port,rep_config);

  redis_server.run();
  redis_server.stop();


  return 0;
}
