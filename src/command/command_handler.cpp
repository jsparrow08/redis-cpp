#include "command_handler.hpp"
#include "string_commands.hpp"
#include "server_commands.hpp"
#include "command_utils.hpp"
#include <algorithm>

CommandHandler::CommandHandler(Config& config)
    : config_(config) {
    initializeRegistry();
}

void CommandHandler::initializeRegistry() {
    // String commands
    registry_["SET"] = commands::string_cmd::set;
    registry_["GET"] = commands::string_cmd::get;
    
    // Server commands
    registry_["PING"] = commands::server_cmd::ping;
    registry_["ECHO"] = commands::server_cmd::echo;
    registry_["INFO"] = commands::server_cmd::info;
    registry_["REPLCONF"] = commands::server_cmd::replconf;
    registry_["PSYNC"] = commands::server_cmd::psync;
}

std::optional<std::string> CommandHandler::handleCommand(int bytes, char buffer[]) {
    std::string input(buffer, bytes);
    
    // Decode RESP
    auto decode_result = resp_parser::decode(input);
    if (!decode_result) {
        return cmd_utils::makeErrorResponse("ERR invalid request");
    }
    
    struct resp_value res = decode_result->first;
    if (res.type != RespType::ARRAY) {
        return cmd_utils::makeErrorResponse("ERR invalid request format");
    }

    auto arr = std::get<std::vector<resp_value>>(res.data);
    if (arr.empty() || arr[0].type != RespType::BULK_STRING) {
        return cmd_utils::makeErrorResponse("ERR invalid command format");
    }
    
    // Extract command name and convert to uppercase for case-insensitive matching
    std::string cmd = std::get<std::string>(arr[0].data);
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);
    
    // Look up command in registry
    auto it = registry_.find(cmd);
    if (it == registry_.end()) {
        return cmd_utils::makeErrorResponse("ERR unknown command '" + cmd + "'");
    }
    
    // Execute command
    return it->second(arr, rd_store_, config_);
}

// New overload: handle command from parsed RESP with source tracking
std::optional<std::string> CommandHandler::handleCommand(
    const std::vector<resp_value>& args, 
    CommandSource source
) {
    if (args.empty() || args[0].type != RespType::BULK_STRING) {
        return cmd_utils::makeErrorResponse("ERR invalid command format");
    }
    
    // Extract command name and convert to uppercase for case-insensitive matching
    std::string cmd = std::get<std::string>(args[0].data);
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);
    
    // Look up command in registry
    auto it = registry_.find(cmd);
    if (it == registry_.end()) {
        return cmd_utils::makeErrorResponse("ERR unknown command '" + cmd + "'");
    }
    

    auto response = it->second(args, rd_store_, config_);
    

    if (source == CommandSource::REPLICATION) {
        
        return std::nullopt;
    }
    
    return response;
}
