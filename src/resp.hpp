#pragma once
#include <string>
#include <vector>
#include <variant>
#include <optional>

enum class RespType {
  SIMPLE_STRING,
  INTEGER,
  ARRAY,
  BULK_STRING,
  ERROR,
  NIL
  //TODO:: Implement for other resp type 
};

struct resp_value {
    using DataVariant = std::variant<
        std::string,             // SimpleString, Error, BulkString
        long long,               // Integer
        std::vector<resp_value>,  // Array
        std::nullptr_t           // Null
        
    >;

    RespType type;
    DataVariant data;

    static resp_value make_string(const std::string& value) {return resp_value{RespType::SIMPLE_STRING, value}; }
    static resp_value make_bulk_string(const std::string& value) {return resp_value{RespType::BULK_STRING, value}; }
    static resp_value make_integer(long long value) {return resp_value{RespType::INTEGER, value}; }
    static resp_value make_array(const std::vector<resp_value>& value) {return resp_value{RespType::ARRAY, value}; }
    static resp_value make_null() {return resp_value{RespType::NIL, nullptr}; }

};

class resp_parser {
    public :
        // encode resp_value
        static std::string encode(const resp_value& input);
        // decode resp_value
        static std::optional<std::pair<resp_value, size_t>> decode(const std::string& buffer);

    private:
        static std::string encode_bulk_str(const std::string& s);
        static std::string encode_array(const std::vector<resp_value>& v);


};



