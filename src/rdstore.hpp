#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <variant>
#include <optional>
#include <chrono>





//  Set parameters
enum SetFlag{
    EX,
    PX
    //TODO: Will implement other argument support later
};

struct set_param{
    long long expiry;
    SetFlag flag;
};

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

//helpers
inline long long get_current_time_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}


class RDStore {
    public:

        RDStore() = default;

        // public apis//
        bool set(std::string &key, std::string &val, struct set_param *param);

        std::optional<std::string> get(std::string &key);

    private:

        std::shared_mutex mt;
        std::unordered_map<std::string,struct RDObj> rd_map;
        std::unordered_map<std::string,long long> expiry_map;

};




