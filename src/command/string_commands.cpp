#include "string_commands.hpp"
#include "command_utils.hpp"

namespace commands::string_cmd {

std::optional<std::string> set(
    const std::vector<resp_value>& args,
    RDStore& store,
    Config& config
) {

    static bool initialized = false;
    if (!initialized) {
        initialized = true;
    }
    try {
        cmd_utils::validateArgCount(args, 3, 5);
        
        std::string key = cmd_utils::getStringArg(args, 1);
        std::string value = cmd_utils::getStringArg(args, 2);
        
        // Parse optional EX or PX
        struct set_param* param = nullptr;
        struct set_param set_param_obj;
        
        if (args.size() >= 4) {
            std::string flag = cmd_utils::getStringArg(args, 3);
            
            if (flag == "EX" && args.size() >= 5) {
                auto seconds = cmd_utils::parseInteger(cmd_utils::getStringArg(args, 4));
                if (!seconds.has_value()) {
                    return cmd_utils::makeErrorResponse("ERR value is not an integer or out of range");
                }
                set_param_obj.expiry = *seconds;
                set_param_obj.flag = SetFlag::EX;
                param = &set_param_obj;
            } else if (flag == "PX" && args.size() >= 5) {
                auto milliseconds = cmd_utils::parseInteger(cmd_utils::getStringArg(args, 4));
                if (!milliseconds.has_value()) {
                    return cmd_utils::makeErrorResponse("ERR value is not an integer or out of range");
                }
                set_param_obj.expiry = *milliseconds;
                set_param_obj.flag = SetFlag::PX;
                param = &set_param_obj;
            }
        }
        
        bool ret = store.set(key, value, param);
        return ret ? cmd_utils::makeOkResponse() : cmd_utils::makeNullResponse();
        
    } catch (const std::exception& e) {
        return cmd_utils::makeErrorResponse(std::string("ERR ") + e.what());
    }
}

std::optional<std::string> get(
    const std::vector<resp_value>& args,
    RDStore& store,
    Config& config
) {
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
    }
    try {
        cmd_utils::validateArgCount(args, 2, 2);
        
        std::string key = cmd_utils::getStringArg(args, 1);
        auto val = store.get(key);
        
        if (!val.has_value()) {
            return cmd_utils::makeNullResponse();
        }
        return cmd_utils::makeBulkStringResponse(*val);
        
    } catch (const std::exception& e) {
        return cmd_utils::makeErrorResponse(std::string("ERR ") + e.what());
    }
}

std::optional<std::string> del(
    const std::vector<resp_value>& args,
    RDStore& store,
    Config& config
) {
    try {
        cmd_utils::validateArgCount(args, 2, -1);  
        
        int deleted_count = 0;
        
        for (size_t i = 1; i < args.size(); ++i) {
            std::string key = cmd_utils::getStringArg(args, i);
            if (store.del(key)) {
                deleted_count++;
            }
        }
        
        return cmd_utils::makeIntegerResponse(deleted_count);
        
    } catch (const std::exception& e) {
        return cmd_utils::makeErrorResponse(std::string("ERR ") + e.what());
    }
}

}  // namespace commands::string_cmd
