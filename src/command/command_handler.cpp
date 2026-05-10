#include "command_handler.hpp"
#include "string_commands.hpp"
#include "server_commands.hpp"
#include "replication_commands.hpp"
#include "command_utils.hpp"
#include <algorithm>

CommandHandler::CommandHandler(Config& config)
    : config_(config), rd_store_(&config) {
    initializeRegistry();
}

void CommandHandler::initializeRegistry() {

    // string commands
    registry_["SET"] = {commands::string_cmd::set, CommandType::STRING};
    registry_["GET"] = {commands::string_cmd::get, CommandType::STRING};
    registry_["DEL"] = {commands::string_cmd::del, CommandType::STRING};
    
    // Server commands
    registry_["PING"] = {commands::server_cmd::ping, CommandType::SERVER};
    registry_["ECHO"] = {commands::server_cmd::echo, CommandType::SERVER};
    registry_["INFO"] = {commands::server_cmd::info, CommandType::SERVER};

    // replicaiton command
    registry_["REPLCONF"] = {commands::replication_cmd::replconf, CommandType::REPLICATION};
    registry_["PSYNC"] = {commands::replication_cmd::psync, CommandType::REPLICATION};
    registry_["WAIT"] = {commands::replication_cmd::wait, CommandType::REPLICATION};
}

// std::optional<std::string> CommandHandler::handleCommand(int bytes, char buffer[]) {
//     std::string input(buffer, bytes);
    
//     // Decode RESP
//     auto decode_result = resp_parser::decode(input);
//     if (!decode_result) {
//         return cmd_utils::makeErrorResponse("ERR invalid request");
//     }
    
//     struct resp_value res = decode_result->first;
//     if (res.type != RespType::ARRAY) {
//         return cmd_utils::makeErrorResponse("ERR invalid request format");
//     }

//     auto arr = std::get<std::vector<resp_value>>(res.data);
//     if (arr.empty() || arr[0].type != RespType::BULK_STRING) {
//         return cmd_utils::makeErrorResponse("ERR invalid command format");
//     }
    
//     // Extract command name and convert to uppercase for case-insensitive matching
//     std::string cmd = std::get<std::string>(arr[0].data);
//     std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);
    
//     // Look up command in registry
//     auto it = registry_.find(cmd);
//     if (it == registry_.end()) {
//         return cmd_utils::makeErrorResponse("ERR unknown command '" + cmd + "'");
//     }
    
//     // Execute command
//     return it->second(arr, rd_store_, config_);
// }

// New overload: handle command from parsed RESP with source tracking
std::optional<std::string> CommandHandler::handleCommand(
    const std::vector<resp_value>& args, 
    CommandSource source
) {
    // 1. Validate basic format
    if (args.empty() || args[0].type != RespType::BULK_STRING) {
        return cmd_utils::makeErrorResponse("ERR invalid command format");
    }
    
    // 2. Route based on the source of the command
    if (source == CommandSource::REPLICATION) {
        return handleReplicationStreamCommand(args);
    }
    
    // --- STANDARD CLIENT PROCESSING ---
    
    std::string cmd = std::get<std::string>(args[0].data);
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);
    
    auto it = registry_.find(cmd);
    if (it == registry_.end()) {
        return cmd_utils::makeErrorResponse("ERR unknown command '" + cmd + "'");
    }
    
    // Execute and return the generated response to the client
    return it->second.function(args, rd_store_, config_);
}


std::optional<std::string> CommandHandler::handleReplicationStreamCommand(
    const std::vector<resp_value>& args
) {
    std::string cmd = std::get<std::string>(args[0].data);
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);
    
    auto it = registry_.find(cmd);
    if (it == registry_.end()) {
        return std::nullopt; 
    }
    
    // Execute the command to update the local Replica state (e.g., store.set, store.del)
    auto response = it->second.function(args, rd_store_, config_);
    
    // The only command that is allowed to reply to the master is REPLCONF GETACK
    if (cmd == "REPLCONF" && args.size() >= 2 && args[1].type == RespType::BULK_STRING) {
        std::string subcommand = std::get<std::string>(args[1].data);
        std::transform(subcommand.begin(), subcommand.end(), subcommand.begin(), ::toupper);
        
        if (subcommand == "GETACK") {
            return response; // Return the ACK 0 string back to the master
        }
    }
    
    // All other propagated commands (SET, DEL, PING) remain completely silent
    return std::nullopt;
}