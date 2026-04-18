#include "resp.hpp"
#include <sstream>

std::string resp_parser::encode(const resp_value& val) {
    switch (val.type) {
        case RespType::SIMPLE_STRING: return "+" + std::get<std::string>(val.data) + "\r\n";
        case RespType::INTEGER:      return ":" + std::to_string(std::get<long long>(val.data)) + "\r\n";
        case RespType::BULK_STRING:   return encode_bulk_str(std::get<std::string>(val.data));
        case RespType::ARRAY:        return encode_array(std::get<std::vector<resp_value>>(val.data));
        case RespType::NIL:         return "$-1\r\n";
        default: return "";
    }
}

std::string resp_parser::encode_bulk_str(const std::string& s) {
    return "$" + std::to_string(s.size()) + "\r\n" + s + "\r\n";
}

std::string resp_parser::encode_array(const std::vector<resp_value>& v) {
    std::string result = "*" + std::to_string(v.size()) + "\r\n";
    for (const auto& val : v) {
        result += encode(val);
    }
    return result;
}

std::optional<std::pair<resp_value, size_t>> resp_parser::decode(const std::string& buffer) {
    if (buffer.empty()) return std::nullopt;

    size_t pos = buffer.find("\r\n");
    if (pos == std::string::npos) return std::nullopt; // Incomplete line

    char type_prefix = buffer[0];
    std::string payload = buffer.substr(1, pos - 1);
    size_t total_consumed = pos + 2;


    if (type_prefix == '+') {
        return std::make_pair(resp_value::make_string(payload), total_consumed);
    } 
    else if (type_prefix == ':') {
        return std::make_pair(resp_value::make_integer(std::stoll(payload)), total_consumed);
    } 
    else if (type_prefix == '$') {
        // Bulk string decoding
        int len = std::stoi(payload);
        if (len == -1) return std::make_pair(resp_value::make_null(), total_consumed);
        
        // Check if we have the full bulk string + terminal CRLF
        if (buffer.length() < total_consumed + len + 2) return std::nullopt;
        
        std::string body = buffer.substr(total_consumed, len);
        return std::make_pair(resp_value::make_string(body), total_consumed + len + 2);
    }
    else if (type_prefix == '*') {
        // --- ARRAY DECODING ---
        int num_elements = std::stoi(payload);
        if (num_elements == -1) return std::make_pair(resp_value::make_null(), total_consumed);
        
        std::vector<resp_value> elements;
        size_t current_pos = total_consumed;

        for (int i = 0; i < num_elements; ++i) {
            // Recursively call decode on the remaining buffer
            auto result = decode(buffer.substr(current_pos));
            
            if (!result) return std::nullopt; // Not enough data for all elements

            elements.push_back(result->first);
            current_pos += result->second; // Advance by bytes consumed by child
        }
        return std::make_pair(resp_value::make_array(elements), current_pos);
    }
    // Array parsing would involve calling decode() recursively for N elements
    return std::nullopt; 
}