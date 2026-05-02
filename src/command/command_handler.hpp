#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <functional>
#include <vector>
#include "../config/config.hpp"
#include "../rdstore/rdstore.hpp"
#include "../resp/resp.hpp"
#include "../server/connection.hpp"

// Command handler function type
enum class CommandType {
    STRING,
    SERVER,
    REPLICATION,
    UNKNOWN
};

using CommandHandlerFunc = std::function<std::optional<std::string>(
    const std::vector<resp_value>&,
    RDStore&,
    Config&
)>;

struct CommandMetadata {
    CommandHandlerFunc function;
    CommandType type;
    bool requires_auth = false; // Example of an extensible property
};


class CommandHandler {
public:
    explicit CommandHandler(Config& config);

    // std::optional<std::string> handleCommand(int bytes, char buffer[]);
    

    std::optional<std::string> handleCommand(
        const std::vector<resp_value>& args, 
        CommandSource source = CommandSource::CLIENT
    );

private:
    void initializeRegistry();
    std::optional<std::string> handleReplicationStreamCommand(
        const std::vector<resp_value>& args
    );

    Config& config_;
    RDStore rd_store_;  
    std::unordered_map<std::string, CommandMetadata> registry_;
};
