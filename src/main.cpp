#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <netdb.h>
#include <unordered_map>
#include "server.hpp"


static int port = 6379;

int main(int argc, char **argv) {

  if(argc>=3 && !std::strcmp(argv[1],"--port"))
      port = std::stoi(argv[2]);

  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  
  RedisServer redis_server(port);

  redis_server.run();
  redis_server.stop();


  return 0;
}
