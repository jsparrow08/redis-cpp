#pragma once

#include <vector>
#include <optional>
#include <string>
#include "../resp/resp.hpp"
#include "../rdstore/rdstore.hpp"
#include "../config/config.hpp"

namespace commands::string_cmd {

// ===== String Commands =====

std::optional<std::string> set(
    const std::vector<resp_value>& args,
    RDStore& store,
    Config& config
);
// Syntax: SET key value [EX seconds|PX milliseconds]
// Response: "+OK" or null (if condition fails)

std::optional<std::string> get(
    const std::vector<resp_value>& args,
    RDStore& store,
    Config& config
);
// Syntax: GET key
// Response: bulk_string or null

std::optional<std::string> del(
    const std::vector<resp_value>& args,
    RDStore& store,
    Config& config
);
// Syntax: DEL key [key ...]
// Response: integer (number of keys deleted)

}  // namespace commands::string_cmd
