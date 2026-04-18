#pragma once

#include <memory>
#include <optional>
#include <string>
#include "../config/config.hpp"
#include "../rdstore/rdstore.hpp"
#include "../resp/resp.hpp"

class CommandHandler {
public:
    explicit CommandHandler(Config& config);

    std::optional<std::string> handleCommand(int bytes, char buffer[]);

private:
    std::string getInfo(ServerInfo flag);

    Config& config_;
    RDStore rd_store_;
};
