#pragma once

#include <string>
#include <cstdint>

enum class ConnectionState {
    HANDSHAKING,           // Replica: performing replication handshake
    RDB_SYNCING,          // Replica: receiving RDB file
    COMMAND_STREAMING,    // Replica: receiving propagated commands
    CLIENT_COMMAND_WAITING, // Client: waiting for command
    REPLY_QUEUED,         // Data queued in output_buffer, ready to flush
    CLOSING               // Connection being closed
};

enum class CommandSource {
    CLIENT,               // Command from regular client
    REPLICATION           // Command propagated from master
};

struct Connection {
    int fd;
    ConnectionState state;
    CommandSource source;  
    
    // I/O Buffers
    std::string input_buffer;      
    std::string output_buffer;     
    
    // Parsing state
    size_t bytes_processed;        
    
    // Replication specific (for master connections)
    bool is_replica_connection;    
    uint64_t rdb_remaining_bytes;  
    
    Connection() 
        : fd(-1), 
          state(ConnectionState::CLIENT_COMMAND_WAITING),
          source(CommandSource::CLIENT),
          bytes_processed(0),
          is_replica_connection(false),
          rdb_remaining_bytes(0) {}
    
    Connection(int fd_, ConnectionState state_, CommandSource source_)
        : fd(fd_),
          state(state_),
          source(source_),
          bytes_processed(0),
          is_replica_connection(false),
          rdb_remaining_bytes(0) {}
};
