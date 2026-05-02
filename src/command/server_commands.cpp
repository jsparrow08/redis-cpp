#include "server_commands.hpp"
#include "command_utils.hpp"

namespace commands::server_cmd {


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


}  // namespace commands::server_cmd
