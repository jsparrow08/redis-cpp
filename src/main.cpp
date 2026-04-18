#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <netdb.h>
#include <unordered_map>
#include "config/config.hpp"
#include "config/replication_config.hpp"
#include "server.hpp"





int main(int argc, char **argv) {
  int port = 6379;
  ReplicationConfig rep_config = MasterConfig();

  for (int i = 1; i < argc; i++){
        if (std::string(argv[i]) == "--port" && i + 1 < argc)
            port = std::atoi(argv[++i]);
        if (std::string(argv[i]) == "--replicaof" && i + 1 < argc) {
            std::string next_arg = argv[++i];
            std::string master_host;
            int master_port = 0;

            size_t sep = next_arg.find(' ');
            if (sep != std::string::npos) {
                master_host = next_arg.substr(0, sep);
                master_port = std::atoi(next_arg.substr(sep + 1).c_str());
            } else if (i + 1 < argc) {
                master_host = next_arg;
                master_port = std::atoi(argv[++i]);
            } else {
                std::cerr << "Invalid --replicaof usage. Expected host port.\n";
                return 1;
            }

            rep_config = SlaveConfig(master_host, master_port);
        }
    }

  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  
  RedisServer redis_server(port, rep_config);

  redis_server.run();
  redis_server.stop();

  return 0;
}
