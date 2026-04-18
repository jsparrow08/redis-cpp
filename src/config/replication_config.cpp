#include "replication_config.hpp"

// MasterConfig implementation
void MasterConfig::printInfo() const {
    std::cout << "Server role: master" << std::endl;
    std::cout << "  Replication ID: " << replication_id << std::endl;
    std::cout << "  Replication Offset: " << replication_offset << std::endl;
}

std::string MasterConfig::getInfo() const {
    std::string info="";
    info += "role:master\n";
    info = info + "master_replid:" + replication_id + "\n";
    info = info + "master_repl_offset:" + std::to_string(replication_offset) + "\n";
    return info;
}



// SlaveConfig implementation
SlaveConfig::SlaveConfig(const std::string& host, int port)
    : master_host(host), master_port(port), replication_offset(0) {}

void SlaveConfig::printInfo() const {
    std::cout << "Server role: slave" << std::endl;
    std::cout << "  Master Host: " << master_host << std::endl;
    std::cout << "  Master Port: " << master_port << std::endl;
    std::cout << "  Replication Offset:" << replication_offset << std::endl;
}
std::string SlaveConfig::getInfo() const {
    std::string info="";
    info += "role:slave\n";
    info = info + "master_host:" + master_host + "\n";
    info = info + "master_port:" + std::to_string(master_port) + "\n";
    info = info + "master_repl_offset:" + std::to_string(replication_offset) + "\n";
    return info;
}
