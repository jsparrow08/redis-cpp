#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <variant>
#include <optional>



enum ValType{
    STRING,
    LIST
    // Todo  Implement other data types 

};

struct RDObj{
    std::string val ;
    // TODO: make it a data varit to be compatible with other types
    ValType val_type;
};

class RDStore {
    public:

        RDStore() = default;

        // public apis//
        bool set(std::string &key, std::string &val );

        std::optional<std::string> get(std::string &key);

    private:

        std::shared_mutex mt;
        std::unordered_map<std::string,struct RDObj> rd_map;

};




