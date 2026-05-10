#pragma once

#include <vector>
#include <optional>
#include <string>
#include "../resp/resp.hpp"
#include "../rdstore/rdstore.hpp"
#include "../config/config.hpp"

namespace commands::replication_cmd {


std::optional<std::string> replconf(
    const std::vector<resp_value>& args,
    RDStore& store,
    Config& config
);
// Syntax: REPLCONF [options...]
// Response: "+OK"

std::optional<std::string> psync(
    const std::vector<resp_value>& args,
    RDStore& store,
    Config& config
);
// Syntax: PSYNC replication-id offset
// Response: "+FULLRESYNC replication-id offset" followed by RDB file

std::optional<std::string> wait(
    const std::vector<resp_value>& args,
    RDStore& store,
    Config& config
);

}  // namespace commands::server_cmd
