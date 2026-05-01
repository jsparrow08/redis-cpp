#include "server_commands.hpp"
#include "command_utils.hpp"

namespace commands::server_cmd {

std::string getRdbFile() {
    // Empty RDB file in hex: 524544495300097FA090000000000000000FF (11 bytes)
    // "REDIS0009" + AUX field + end marker
    std::string rdb_hex = "524544495330303131fa0972656469732d76657205372e322e30fa0a72656469732d62697473c040fa056374696d65c26d08bc65fa08757365642d6d656dc2b0c41000fa08616f662d62617365c000fff06e3bfec0ff5aa2";
    std::string rdb_binary;
    
    // Convert hex string to binary
    for (size_t i = 0; i < rdb_hex.length(); i += 2) {
        std::string byte_hex = rdb_hex.substr(i, 2);
        unsigned char byte = static_cast<unsigned char>(std::stoi(byte_hex, nullptr, 16));
        rdb_binary += byte;
    }
    
    return rdb_binary;
}

std::optional<std::string> ping(
    const std::vector<resp_value>& args,
    RDStore& store,
    Config& config
) {
    try {
        cmd_utils::validateArgCount(args, 1, 2);
        
        if (args.size() == 2) {
            // PING message - echo the message
            std::string message = cmd_utils::getStringArg(args, 1);
            return cmd_utils::makeBulkStringResponse(message);
        }
        
        // PING with no arguments - return PONG
        return resp_parser::encode(resp_value::make_string("PONG"));
        
    } catch (const std::exception& e) {
        return cmd_utils::makeErrorResponse(std::string("ERR ") + e.what());
    }
}

std::optional<std::string> echo(
    const std::vector<resp_value>& args,
    RDStore& store,
    Config& config
) {
    try {
        cmd_utils::validateArgCount(args, 2, 2);
        
        std::string message = cmd_utils::getStringArg(args, 1);
        return cmd_utils::makeBulkStringResponse(message);
        
    } catch (const std::exception& e) {
        return cmd_utils::makeErrorResponse(std::string("ERR ") + e.what());
    }
}

std::optional<std::string> info(
    const std::vector<resp_value>& args,
    RDStore& store,
    Config& config
) {
    try {
        cmd_utils::validateArgCount(args, 1, 2);
        
        ServerInfo flag = ServerInfo::ALL;
        if (args.size() == 2) {
            std::string section = cmd_utils::getStringArg(args, 1);
            if (section == "replication") {
                flag = ServerInfo::REPLICATION;
            } else if (section == "clients") {
                flag = ServerInfo::CLIENTS;
            }
        }
        
        // Build info response
        std::string info_body;
        
        if (flag & ServerInfo::SERVER) {
            info_body += "redis_version::" + std::to_string(config.getVersion()) + "\r\n";
        }
        if (flag & ServerInfo::CLIENTS) {
            info_body += "connected_clients:" + std::to_string(config.getClientConnected()) + "\r\n";
        }
        if (flag & ServerInfo::MEMORY) {
            info_body += "used_memory:" + std::to_string(config.getUsedMemory()) + "\r\n";
        }
        if (flag & ServerInfo::REPLICATION) {
            info_body += config.getRepInfo();
        }
        
        return cmd_utils::makeBulkStringResponse(info_body);
        
    } catch (const std::exception& e) {
        return cmd_utils::makeErrorResponse(std::string("ERR ") + e.what());
    }
}

std::optional<std::string> replconf(
    const std::vector<resp_value>& args,
    RDStore& store,
    Config& config
) {
    try {
        // REPLCONF accepts various options, but we just return OK for now
        return cmd_utils::makeOkResponse();
        
    } catch (const std::exception& e) {
        return cmd_utils::makeErrorResponse(std::string("ERR ") + e.what());
    }
}

std::optional<std::string> psync(
    const std::vector<resp_value>& args,
    RDStore& store,
    Config& config
) {
    try {
        cmd_utils::validateArgCount(args, 3, 3);
        
        const auto& replication_config = config.getReplicationConfig();
        if (std::holds_alternative<MasterConfig>(replication_config)) {
            auto master_config = std::get<MasterConfig>(replication_config);
            std::string repl_id = master_config.getReplicationId();
            std::string fullresync = "FULLRESYNC " + repl_id + " 0";
            std::string response = resp_parser::encode(resp_value::make_string(fullresync));
            
            // Append RDB file after FULLRESYNC
            std::string rdb = getRdbFile();
            response += "$" + std::to_string(rdb.length()) + "\r\n" + rdb;
            
            return response;
        } else {
            return cmd_utils::makeErrorResponse("ERR PSYNC only supported on master");
        }
        
    } catch (const std::exception& e) {
        return cmd_utils::makeErrorResponse(std::string("ERR ") + e.what());
    }
}

}  // namespace commands::server_cmd
