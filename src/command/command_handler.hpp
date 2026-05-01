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
using CommandHandlerFunc = std::function<std::optional<std::string>(
    const std::vector<resp_value>&,
    RDStore&,
    Config&
)>;

class CommandHandler {
public:
    explicit CommandHandler(Config& config);

    std::optional<std::string> handleCommand(int bytes, char buffer[]);
    
    // Handle command with source tracking (for async/buffered responses)
    // Returns the response to be queued (or empty if no response should be sent)
    std::optional<std::string> handleCommand(
        const std::vector<resp_value>& args, 
        CommandSource source = CommandSource::CLIENT
    );

private:
    void initializeRegistry();

    Config& config_;
    RDStore rd_store_;
    std::unordered_map<std::string, CommandHandlerFunc> registry_;
};
