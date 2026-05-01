#pragma once

#include <vector>
#include <optional>
#include <string>
#include <cstdint>
#include "../resp/resp.hpp"

namespace cmd_utils {

// ===== Parameter Extraction =====
std::string getStringArg(const std::vector<resp_value>& args, size_t index);
std::optional<std::string> tryGetStringArg(const std::vector<resp_value>& args, size_t index);

// ===== Argument Validation =====
void validateArgCount(const std::vector<resp_value>& args, size_t min, size_t max = SIZE_MAX);

// ===== String to Integer Conversion =====
std::optional<long long> parseInteger(const std::string& str);

// ===== Error Response Helpers =====
std::string makeErrorResponse(const std::string& message);
std::string makeOkResponse();
std::string makeNullResponse();
std::string makeIntegerResponse(long long value);
std::string makeBulkStringResponse(const std::string& value);

}  // namespace cmd_utils
