#pragma once

#include <vector>
#include <optional>
#include <string>
#include "../resp/resp.hpp"
#include "../rdstore/rdstore.hpp"
#include "../config/config.hpp"

namespace commands::server_cmd {

// ===== Server Commands =====

std::optional<std::string> ping(
    const std::vector<resp_value>& args,
    RDStore& store,
    Config& config
);
// Syntax: PING [message]
// Response: "+PONG" or bulk_string if message provided

std::optional<std::string> echo(
    const std::vector<resp_value>& args,
    RDStore& store,
    Config& config
);
// Syntax: ECHO message
// Response: bulk_string (echoes the message)

std::optional<std::string> info(
    const std::vector<resp_value>& args,
    RDStore& store,
    Config& config
);
// Syntax: INFO [section]
// Response: bulk_string (server info)


}  // namespace commands::server_cmd
