#include "server_commands.hpp"
#include "command_utils.hpp"

namespace commands::replication_cmd {

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

std::optional<std::string> replconf(
    const std::vector<resp_value>& args,
    RDStore& store,
    Config& config
) {
    try {
        cmd_utils::validateArgCount(args, 2, 3);
        
        // Get the subcommand
        std::string subcommand = cmd_utils::getStringArg(args, 1);
        std::string subcommand_upper = subcommand;
        std::transform(subcommand_upper.begin(), subcommand_upper.end(), subcommand_upper.begin(), ::toupper);
        
        // Handle GETACK command
        if (subcommand_upper == "GETACK") {

            long long offset = config.getReplicaOffset();
            std::string offset_str = std::to_string(offset);
            
            // Build RESP array response: REPLCONF ACK <offset>
            std::vector<resp_value> ack_response;
            ack_response.push_back(resp_value::make_bulk_string("REPLCONF"));
            ack_response.push_back(resp_value::make_bulk_string("ACK"));
            ack_response.push_back(resp_value::make_bulk_string(offset_str));
            
            std::string response = resp_parser::encode(resp_value::make_array(ack_response));
            
            return response;
        }
        
        // For other REPLCONF commands (listening-port, capa), just return OK
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
            response += "$" + std::to_string(rdb.length()) + "\r\n" + rdb ;
            
            return response;
        } else {
            return cmd_utils::makeErrorResponse("ERR PSYNC only supported on master");
        }
        
    } catch (const std::exception& e) {
        return cmd_utils::makeErrorResponse(std::string("ERR ") + e.what());
    }
}

std::optional<std::string> wait(
    const std::vector<resp_value>& args,
    RDStore& store,
    Config& config
) {
        try {
        cmd_utils::validateArgCount(args, 3, 3);
        std::string response = cmd_utils::makeIntegerResponse(0);
        return response;
        
        
    } catch (const std::exception& e) {
        return cmd_utils::makeErrorResponse(std::string("ERR ") + e.what());
    }

}

}  // namespace commands::server_cmd
