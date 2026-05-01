# Current Implementation Architecture Diagrams

## High-Level System Architecture

```mermaid
graph TB
    subgraph "Client Layer"
        C1["Client 1"]
        C2["Client 2"]
        CN["Client N"]
    end

    subgraph "Network Layer"
        POLL["Poll System Call<br/>I/O Multiplexing"]
        SOCKET["Raw Sockets<br/>Non-blocking"]
    end

    subgraph "Processing Layer"
        SERVER["RedisServer<br/>Single Thread"]
        PARSER["RESP Parser<br/>Synchronous"]
        STORE["RDStore<br/>In-Memory"]
        REPL_MGR["ReplicationManager<br/>Handshake & Sync"]
    end

    subgraph "Data Layer"
        KV_MAP["unordered_map<br/>Key-Value Store"]
        EXPIRY_MAP["unordered_map<br/>Expiry Tracking"]
        MUTEX["Shared Mutex<br/>Thread Safety"]
    end

    C1 -->|TCP Connection| POLL
    C2 -->|TCP Connection| POLL
    CN -->|TCP Connection| POLL

    POLL -->|Events| SERVER
    SERVER -->|Parse Request| PARSER
    PARSER -->|Command + Args| SERVER
    SERVER -->|Execute| STORE
    SERVER -->|Init Replica| REPL_MGR

    STORE -->|Read/Write| KV_MAP
    STORE -->|Read/Write| EXPIRY_MAP
    STORE -->|Protect| MUTEX

    REPL_MGR -->|Connect to Master| SOCKET
    REPL_MGR -->|Send/Recv Handshake| POLL

    SERVER -->|Response| C1
    SERVER -->|Response| C2
    SERVER -->|Response| CN
```

## Component Interaction Flow

```mermaid
sequenceDiagram
    participant Client
    participant Poll
    participant RedisServer
    participant RESP_Parser
    participant RDStore
    participant ReplicationManager
    participant Mutex

    Client->>Poll: Send RESP Request
    Poll->>RedisServer: Notify Readable FD
    RedisServer->>RedisServer: recv() into buffer[1024]
    RedisServer->>RESP_Parser: decode(input)
    RESP_Parser->>RedisServer: Return parsed command
    RedisServer->>RDStore: Execute command
    RDStore->>Mutex: Acquire lock
    Mutex->>RDStore: Lock granted
    RDStore->>RDStore: Access data maps
    RDStore->>Mutex: Release lock
    RDStore->>RedisServer: Return result
    RedisServer->>RedisServer: Encode response
    RedisServer->>Client: send() response

    Note over RedisServer,ReplicationManager: Replica Mode Only
    RedisServer->>ReplicationManager: start_handshake()
    ReplicationManager->>ReplicationManager: connectToMaster()
    ReplicationManager->>ReplicationManager: sendPing()
    ReplicationManager->>ReplicationManager: receivePong()
    ReplicationManager->>RedisServer: Handshake result
```

## Data Type Architecture

```mermaid
graph LR
    RDStore["RDStore Class"]

    subgraph "Data Structures"
        KV["unordered_map<string, RDObj>"]
        EXPIRY["unordered_map<string, long long>"]
    end

    subgraph "RDObj Structure"
        VAL["string val"]
        TYPE["ValType (STRING only)"]
    end

    subgraph "Expiry Management"
        PASSIVE["Passive Expiry<br/>On Access Only"]
        NO_CLEANUP["No Active Cleanup"]
    end

    RDStore -->|Stores| KV
    RDStore -->|Tracks| EXPIRY
    KV -->|Contains| VAL
    KV -->|Contains| TYPE
    EXPIRY -->|Uses| PASSIVE
    EXPIRY -->|No| NO_CLEANUP
```

## Thread Model

```mermaid
graph TB
    MAIN["Main Thread<br/>Single Threaded"]

    subgraph "Event Loop"
        POLL_LOOP["Poll Loop<br/>while(true)"]
        ACCEPT_LOOP["Accept Loop<br/>while(true)"]
        PROCESS_LOOP["Process Clients<br/>Sequential"]
        REPL_INIT["Replication Init<br/>Before Loop"]
    end

    subgraph "Operations"
        RECV["recv() calls<br/>Non-blocking"]
        PARSE["Parse RESP<br/>Synchronous"]
        EXECUTE["Execute Commands<br/>Sequential"]
        SEND["send() calls<br/>Non-blocking"]
        HANDSHAKE["Handshake Ops<br/>Blocking"]
    end

    MAIN -->|Runs| REPL_INIT
    REPL_INIT -->|Then| POLL_LOOP
    POLL_LOOP -->|Handles| ACCEPT_LOOP
    POLL_LOOP -->|Handles| PROCESS_LOOP

    PROCESS_LOOP -->|Calls| RECV
    PROCESS_LOOP -->|Calls| PARSE
    PROCESS_LOOP -->|Calls| EXECUTE
    PROCESS_LOOP -->|Calls| SEND
    REPL_INIT -->|Calls| HANDSHAKE
```

## Memory Layout

```mermaid
graph TB
    subgraph "Heap Memory"
        SERVER_OBJ["RedisServer Object"]
        STORE_OBJ["RDStore Object"]
        CONFIG_OBJ["ServerConfig Object"]
        KV_DATA["unordered_map data<br/>Key-Value pairs"]
        EXPIRY_DATA["unordered_map data<br/>Expiry timestamps"]
    end

    subgraph "Stack Memory"
        BUFFER["char buffer[1024]<br/>Per client read"]
        TEMP_VARS["Temporary Variables<br/>Command processing"]
    end

    subgraph "Static/Global"
        MUTEX_OBJ["shared_mutex<br/>Single instance"]
    end

    SERVER_OBJ -->|Contains| STORE_OBJ
    SERVER_OBJ -->|Contains| CONFIG_OBJ
    STORE_OBJ -->|Contains| KV_DATA
    STORE_OBJ -->|Contains| EXPIRY_DATA
    STORE_OBJ -->|Contains| MUTEX_OBJ
```

## Command Processing Pipeline

```mermaid
graph LR
    INPUT["Raw TCP Data<br/>Up to 1024 bytes"]
    RECV["recv() into buffer"]
    STRING["Convert to string"]
    DECODE["RESP decode()"]
    VALIDATE["Check ARRAY type"]
    EXTRACT["Extract command<br/>and args"]
    ROUTE["Switch on command<br/>string"]
    EXECUTE["Call RDStore methods"]
    ENCODE["RESP encode()"]
    SEND["send() to client"]

    INPUT -->|TCP Stream| RECV
    RECV -->|Bytes| STRING
    STRING -->|String| DECODE
    DECODE -->|resp_value| VALIDATE
    VALIDATE -->|Valid| EXTRACT
    EXTRACT -->|Command| ROUTE
    ROUTE -->|SET/GET/etc| EXECUTE
    EXECUTE -->|Result| ENCODE
    ENCODE -->|RESP bytes| SEND
```

## Network Architecture

```mermaid
graph TB
    subgraph "Socket Layer"
        SERVER_SOCKET["Server Socket<br/>server_fd"]
        CLIENT_SOCKETS["Client Sockets<br/>client_fd[1..N]"]
    end

    subgraph "I/O Multiplexing"
        POLL_FDS["pollfd array<br/>Server + clients"]
        POLLIN_EVENTS["POLLIN events<br/>Readable data"]
    end

    subgraph "Connection Management"
        ACCEPT["accept() calls<br/>In loop"]
        CLOSE["close() calls<br/>On errors"]
        NONBLOCK["O_NONBLOCK flag<br/>All sockets"]
    end

    SERVER_SOCKET -->|Added to| POLL_FDS
    CLIENT_SOCKETS -->|Added to| POLL_FDS
    POLL_FDS -->|Generates| POLLIN_EVENTS
    POLLIN_EVENTS -->|Triggers| ACCEPT
    POLLIN_EVENTS -->|Triggers| CLOSE
    ACCEPT -->|Creates| CLIENT_SOCKETS
    CLOSE -->|Removes from| POLL_FDS
```

## Storage Access Pattern

```mermaid
graph TB
    subgraph "Read Operations (GET)"
        READ_LOCK["shared_lock<br/>Reader lock"]
        FIND_KEY["find() in rd_map"]
        CHECK_EXPIRY["Check expiry_map"]
        CALC_TIME["get_current_time_ms()"]
        ERASE_EXPIRED["erase() if expired"]
        RETURN_VALUE["Return value or nullopt"]
    end

    subgraph "Write Operations (SET)"
        WRITE_LOCK["unique_lock<br/>Writer lock"]
        INSERT_KV["rd_map[key] = value"]
        INSERT_EXPIRY["expiry_map[key] = time"]
        RETURN_TRUE["Return true"]
    end

    READ_LOCK -->|Allows| FIND_KEY
    FIND_KEY -->|Found| CHECK_EXPIRY
    CHECK_EXPIRY -->|Expired| ERASE_EXPIRED
    CHECK_EXPIRY -->|Valid| RETURN_VALUE
    ERASE_EXPIRED -->|Return| RETURN_VALUE

    WRITE_LOCK -->|Allows| INSERT_KV
    INSERT_KV -->|Then| INSERT_EXPIRY
    INSERT_EXPIRY -->|Then| RETURN_TRUE
```

## Error Handling Flow

```mermaid
graph TD
    START["Client Request"]
    RECV_ERROR["recv() < 0"]
    PARSE_ERROR["decode() fails"]
    INVALID_CMD["Unknown command"]
    ARGS_ERROR["Wrong arguments"]
    STORE_ERROR["Storage operation fails"]
    ENCODE_ERROR["encode() fails"]
    SEND_ERROR["send() fails"]
    REPL_ERROR["Replication handshake fails"]
    CLOSE_CLIENT["Close connection"]
    SHUTDOWN["Server shutdown"]

    START -->|Success| RECV_ERROR
    RECV_ERROR -->|Error| CLOSE_CLIENT
    RECV_ERROR -->|Success| PARSE_ERROR
    PARSE_ERROR -->|Error| CLOSE_CLIENT
    PARSE_ERROR -->|Success| INVALID_CMD
    INVALID_CMD -->|Unknown| CLOSE_CLIENT
    INVALID_CMD -->|Known| ARGS_ERROR
    ARGS_ERROR -->|Invalid| CLOSE_CLIENT
    ARGS_ERROR -->|Valid| STORE_ERROR
    STORE_ERROR -->|Error| CLOSE_CLIENT
    STORE_ERROR -->|Success| ENCODE_ERROR
    ENCODE_ERROR -->|Error| CLOSE_CLIENT
    ENCODE_ERROR -->|Success| SEND_ERROR
    SEND_ERROR -->|Error| CLOSE_CLIENT
    SEND_ERROR -->|Success| START

    REPL_ERROR -->|On startup| SHUTDOWN
```

## Configuration Architecture

```mermaid
graph LR
    subgraph "Command Line Args"
        PORT_ARG["--port <num>"]
        REPLICA_ARG["--replicaof <host> <port>"]
    end

    subgraph "Configuration Objects"
        SERVER_CONFIG["ServerConfig struct"]
        REPLICA_CONFIG["ReplicationConfig struct"]
    end

    subgraph "Runtime State"
        CONNECTED_CLIENTS["client_connected counter"]
        SERVER_ROLE["role string"]
        USED_MEMORY["used_memory counter"]
    end

    PORT_ARG -->|Parsed| SERVER_CONFIG
    REPLICA_ARG -->|Parsed| REPLICA_CONFIG
    REPLICA_CONFIG -->|Sets| SERVER_ROLE
    SERVER_CONFIG -->|Tracks| CONNECTED_CLIENTS
    SERVER_CONFIG -->|Tracks| USED_MEMORY
```

## Replication Architecture

```mermaid
graph TB
    subgraph "Master Server"
        MASTER_SERVER["RedisServer<br/>Role: master"]
        MASTER_STORE["RDStore<br/>Master data"]
        MASTER_POLL["Poll Loop<br/>Accepts replicas"]
        MASTER_RDB["RDB Generator<br/>Empty RDB Hex"]
    end

    subgraph "Replica Server"
        REPLICA_SERVER["RedisServer<br/>Role: slave"]
        REPLICA_MGR["ReplicationManager"]
        REPLICA_STORE["RDStore<br/>Replica data"]
    end

    subgraph "Handshake Protocol"
        CONNECT["TCP Connect<br/>to Master"]
        SEND_PING["Send PING<br/>Receive PONG"]
        SEND_REPLCONF["Send REPLCONF<br/>port & capa"]
        SEND_PSYNC["Send PSYNC<br/>? -1"]
        RECV_FULLRESYNC["Receive FULLRESYNC<br/>+ RDB File"]
        SUCCESS["Handshake Success<br/>Sync complete"]
    end

    REPLICA_SERVER -->|Init| REPLICA_MGR
    REPLICA_MGR -->|1| CONNECT
    CONNECT -->|2| SEND_PING
    SEND_PING -->|3| SEND_REPLCONF
    SEND_REPLCONF -->|4| SEND_PSYNC
    SEND_PSYNC -->|5| RECV_FULLRESYNC
    RECV_FULLRESYNC -->|6| SUCCESS

    MASTER_POLL -->|Accept| MASTER_SERVER
    MASTER_SERVER -->|Handle PING| MASTER_STORE
    MASTER_STORE -->|Return PONG| MASTER_SERVER
    MASTER_SERVER -->|Handle REPLCONF| MASTER_STORE
    MASTER_SERVER -->|Handle PSYNC| MASTER_RDB
    MASTER_RDB -->|Return FULLRESYNC + RDB| MASTER_SERVER
    MASTER_SERVER -->|Send Responses| REPLICA_MGR
```

<!-- 
## Limitations Overview

```mermaid
graph TD
    subgraph "Performance Issues"
        SINGLE_THREAD["Single Thread<br/>No concurrency"]
        BLOCKING_IO["Blocking recv/send<br/>Per client"]
        NO_THREAD_POOL["No worker threads"]
    end

    subgraph "Scalability Issues"
        POLL_LIMIT["Poll FD limit<br/>~1000 connections"]
        FIXED_BUFFER["1024 byte buffer<br/>Message size limit"]
        NO_ASYNC["No async operations"]
    end

    subgraph "Reliability Issues"
        NO_PERSISTENCE["No data persistence"]
        PASSIVE_EXPIRY["No active expiry cleanup"]
        BASIC_ERROR_HANDLING["Limited error recovery"]
    end

    subgraph "Security Issues"
        NO_AUTH["No authentication"]
        NO_RATE_LIMIT["No rate limiting"]
        BUFFER_OVERFLOW["Potential buffer overflow"]
    end
``` -->
