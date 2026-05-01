#include "command_utils.hpp"
#include <stdexcept>
#include <limits>

namespace cmd_utils {

std::string getStringArg(const std::vector<resp_value>& args, size_t index) {
    if (index >= args.size()) {
        throw std::out_of_range("Argument index out of range");
    }
    if (args[index].type != RespType::BULK_STRING && args[index].type != RespType::SIMPLE_STRING) {
        throw std::invalid_argument("Argument is not a string");
    }
    return std::get<std::string>(args[index].data);
}

std::optional<std::string> tryGetStringArg(const std::vector<resp_value>& args, size_t index) {
    if (index >= args.size()) {
        return std::nullopt;
    }
    if (args[index].type != RespType::BULK_STRING && args[index].type != RespType::SIMPLE_STRING) {
        return std::nullopt;
    }
    return std::get<std::string>(args[index].data);
}

void validateArgCount(const std::vector<resp_value>& args, size_t min, size_t max) {
    if (args.size() < min || args.size() > max) {
        throw std::invalid_argument("Invalid argument count");
    }
}

std::optional<long long> parseInteger(const std::string& str) {
    try {
        size_t pos = 0;
        long long value = std::stoll(str, &pos);
        if (pos != str.length()) {
            return std::nullopt;  // Not a complete integer
        }
        return value;
    } catch (const std::exception&) {
        return std::nullopt;  // Overflow or invalid format
    }
}

std::string makeErrorResponse(const std::string& message) {
    return resp_parser::encode(resp_value::make_error(message));
}

std::string makeOkResponse() {
    return resp_parser::encode(resp_value::make_string("OK"));
}

std::string makeNullResponse() {
    return resp_parser::encode(resp_value::make_null());
}

std::string makeIntegerResponse(long long value) {
    return resp_parser::encode(resp_value::make_integer(value));
}

std::string makeBulkStringResponse(const std::string& value) {
    return resp_parser::encode(resp_value::make_bulk_string(value));
}

}  // namespace cmd_utils
