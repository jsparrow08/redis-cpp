#include "command_handler.hpp"

CommandHandler::CommandHandler(Config& config)
    : config_(config) {}

std::optional<std::string> CommandHandler::handleCommand(int bytes, char buffer[]) {
    std::string input(buffer, bytes);
    struct resp_value res = resp_parser::decode(input)->first;
    if (res.type != RespType::ARRAY) {
        return std::nullopt;
    }

    auto arr = std::get<std::vector<resp_value>>(res.data);
    if (arr.empty() || arr[0].type != RespType::SIMPLE_STRING) {
        return std::nullopt;
    }

    std::string cmd = std::get<std::string>(arr[0].data);
    std::string response;
    // std::cout<<"cmd : "<<cmd<<"\n";

    if (cmd == "PING") {
        response = resp_parser::encode(resp_value::make_string("PONG"));
    } else if (cmd == "ECHO") {
        if (arr.size() < 2) {
            return std::nullopt;
        }
        std::string echo_str = std::get<std::string>(arr[1].data);
        response = resp_parser::encode(resp_value::make_bulk_string(echo_str));
    } else if (cmd == "SET") {
        if (arr.size() < 3) {
            return std::nullopt;
        }
        auto st_param_opt = get_set_params(arr);
        bool ret;
        if (st_param_opt.has_value()) {
            ret = rd_store_.set(std::get<std::string>(arr[1].data),
                                std::get<std::string>(arr[2].data),
                                &st_param_opt.value());
        } else {
            ret = rd_store_.set(std::get<std::string>(arr[1].data),
                                std::get<std::string>(arr[2].data),
                                nullptr);
        }
        response = ret ? resp_parser::encode(resp_value::make_string("OK"))
                       : resp_parser::encode(resp_value::make_null());
    } else if (cmd == "GET") {
        if (arr.size() < 2) {
            return std::nullopt;
        }
        auto val = rd_store_.get(std::get<std::string>(arr[1].data));
        if (!val.has_value()) {
            response = resp_parser::encode(resp_value::make_null());
        } else {
            response = resp_parser::encode(resp_value::make_bulk_string(*val));
        }
    } else if (cmd == "INFO") {
        if (arr.size() > 1 && std::get<std::string>(arr[1].data) == "replication") {
            response = getInfo(ServerInfo::REPLICATION);
        } else if (arr.size() > 1 && std::get<std::string>(arr[1].data) == "clients") {
            response = getInfo(ServerInfo::CLIENTS);
        } else {
            response = getInfo(ServerInfo::ALL);
        }
    } else if (cmd == "REPLCONF") {
        response = resp_parser::encode(resp_value::make_string("OK"));
    } else if (cmd == "PSYNC") {
        const auto& replication_config = config_.getReplicationConfig();
        if (std::holds_alternative<MasterConfig>(replication_config)) {
            auto master_config = std::get<MasterConfig>(replication_config);
            std::string repl_id = master_config.getReplicationId();
            std::string fullresync = "FULLRESYNC " + repl_id + " 0";
            response = resp_parser::encode(resp_value::make_string(fullresync));
        } else {
            return std::nullopt;  // Only masters can handle PSYNC
        }
    } 
    else {
        return std::nullopt;
    }

    return response;
}

std::string CommandHandler::getInfo(ServerInfo flag) {
    std::string info_body;

    if (flag & ServerInfo::SERVER) {
        info_body += "redis_version::" + std::to_string(config_.getVersion()) + "\r\n";
    }
    if (flag & ServerInfo::CLIENTS) {
        info_body += "connected_clients:" + std::to_string(config_.getClientConnected()) + "\r\n";
    }
    if (flag & ServerInfo::MEMORY) {
        info_body += "used_memory:" + std::to_string(config_.getUsedMemory()) + "\r\n";
    }
    if (flag & ServerInfo::REPLICATION) {
        info_body += config_.getRepInfo();
    }

    return resp_parser::encode(resp_value::make_bulk_string(info_body));
}
