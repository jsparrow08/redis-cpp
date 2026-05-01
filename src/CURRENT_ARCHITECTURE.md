# Current Implementation Architecture Diagrams

## High-Level System Architecture (Async Redis-Style)

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

    subgraph "Connection Management"
        CONNMAP["std::map<fd, Connection><br/>All active connections"]
        MASTER_FD["master_fd<br/>Replica to Master"]
    end

    subgraph "Processing Layer"
        SERVER["RedisServer<br/>Async Event Loop"]
        CMD_HANDLER["CommandHandler<br/>Sync execution"]
        STORE["RDStore<br/>In-Memory"]
        REPL_MGR["ReplicationManager<br/>Handshake & Sync"]
    end

    subgraph "Data Layer"
        KV_MAP["unordered_map<br/>Key-Value Store"]
        EXPIRY_MAP["unordered_map<br/>Expiry Tracking"]
        MUTEX["Shared Mutex<br/>Thread Safety"]
    end

    C1 -->|TCP| POLL
    C2 -->|TCP| POLL
    CN -->|TCP| POLL

    POLL -->|Events| SERVER
    SERVER -->|Manage| CONNMAP
    SERVER -->|Track| MASTER_FD
    
    SERVER -->|Parse & Execute| CMD_HANDLER
    CMD_HANDLER -->|Read/Write| STORE
    SERVER -->|Init| REPL_MGR
    REPL_MGR -->|Connect| MASTER_FD

    STORE -->|Access| KV_MAP
    STORE -->|Track| EXPIRY_MAP
    STORE -->|Protect| MUTEX

    SERVER -->|Queue Response| CONNMAP
    CONNMAP -->|Send| C1
    CONNMAP -->|Send| C2
    CONNMAP -->|Send| CN
```

## Connection State Machine

```mermaid
stateDiagram-v2
    [*] --> ACCEPTED: accept()
    
    ACCEPTED --> CLIENT_COMMAND_WAITING: Set source=CLIENT
    ACCEPTED --> HANDSHAKING: Replica connection
    
    CLIENT_COMMAND_WAITING --> CLIENT_COMMAND_WAITING: Process commands
    CLIENT_COMMAND_WAITING --> COMMAND_STREAMING: PSYNC detected
    
    HANDSHAKING --> RDB_SYNCING: Replica after handshake
    RDB_SYNCING --> COMMAND_STREAMING: RDB complete
    
    COMMAND_STREAMING --> COMMAND_STREAMING: Receive propagated cmds
    
    CLIENT_COMMAND_WAITING --> CLOSING: Error or disconnect
    COMMAND_STREAMING --> CLOSING: Error or disconnect
    
    CLOSING --> [*]: close(fd)
```

## Async Event Loop Pipeline

```mermaid
graph LR
    POLL["poll() on all fds"]
    READABLE["Handle POLLIN<br/>Readable events"]
    WRITABLE["Handle POLLOUT<br/>Writable events"]
    
    READ_FN["process_readable_connection()"]
    PARSE["try_parse_command()"]
    EXEC["handleCommand()"]
    QUEUE["queue_response()"]
    
    WRITE_FN["process_writable_connection()"]
    FLUSH["flush_output_buffer()"]
    SEND["send() bytes"]
    
    POLL -->|Active| READABLE
    POLL -->|Active| WRITABLE
    
    READABLE -->|Read data| READ_FN
    READ_FN -->|Accumulate| PARSE
    PARSE -->|Complete RESP| EXEC
    EXEC -->|Result| QUEUE
    QUEUE -->|Update buffers| POLL
    
    WRITABLE -->|Has data| WRITE_FN
    WRITE_FN -->|Drain| FLUSH
    FLUSH -->|Partial| SEND
    SEND -->|Erase sent| POLL
```

## Connection Object Architecture

```mermaid
graph TB
    subgraph "Connection struct"
        FD["int fd<br/>File descriptor"]
        STATE["ConnectionState state<br/>Current state"]
        SOURCE["CommandSource source<br/>CLIENT or REPLICATION"]
        IN_BUF["string input_buffer<br/>Accumulated incoming RESP"]
        OUT_BUF["string output_buffer<br/>Queued responses"]
        IS_REPLICA["bool is_replica_connection<br/>Marks replica connections"]
        RDB_BYTES["uint64_t rdb_remaining_bytes<br/>RDB sync tracking"]
        BYTES_PROC["size_t bytes_processed<br/>Parse state"]
    end
    
    MAP["std::map<int fd, Connection>"]
    MAP -->|Stores| FD
    MAP -->|Stores| STATE
    MAP -->|Stores| SOURCE
    MAP -->|Stores| IN_BUF
    MAP -->|Stores| OUT_BUF
    MAP -->|Stores| IS_REPLICA
    MAP -->|Stores| RDB_BYTES
    MAP -->|Stores| BYTES_PROC
```

## Data Flow: Client Write Command Propagation

```mermaid
sequenceDiagram
    participant Client as Redis-CLI
    participant Master as Master Server
    participant Replica as Replica Server
    
    Note over Client,Master: 1. Client sends SET command
    Client->>Master: SET foo 1 (RESP)
    Master->>Master: recv() into Connection.input_buffer
    Master->>Master: try_parse_command() → RESP array
    Master->>Master: handleCommand(args, CLIENT)
    Master->>Master: RDStore.set(foo, 1)
    Master->>Master: queue_response(+OK)
    Master->>Client: send(+OK)
    
    Note over Master,Replica: 2. Master propagates to replica
    Master->>Master: is_write_cmd = true (SET)
    Master->>Master: propagate_write_command(args)
    Master->>Master: Find replica connections
    Master->>Master: queue_response(replica_fd, SET cmd)
    
    Note over Replica: 3. Replica receives on master_fd
    Replica->>Replica: poll() detects master_fd readable
    Replica->>Replica: recv() into Connection.input_buffer
    Replica->>Replica: try_parse_command() → RESP array
    Replica->>Replica: process_replicated_command()
    Replica->>Replica: handleCommand(args, REPLICATION)
    Replica->>Replica: RDStore.set(foo, 1) - no response queued
```

## Buffered I/O Architecture

```mermaid
graph TB
    subgraph "Read Path"
        RECV_CALL["recv() non-blocking"]
        APPEND["Append to<br/>input_buffer"]
        PARSE_LOOP["While buffer not empty"]
        TRY_PARSE["try_parse_command()"]
        HAS_COMPLETE["Complete RESP?"]
        ERASE_BYTES["Erase parsed bytes<br/>from buffer"]
        EXECUTE["Execute command"]
        QUEUE_RESP["queue_response()"]
    end
    
    subgraph "Write Path"
        HAS_DATA["output_buffer<br/>not empty?"]
        SEND_CALL["send() partial"]
        ERASE_SENT["Erase sent bytes<br/>from buffer"]
        RETRY["Wait for<br/>POLLOUT again"]
    end
    
    RECV_CALL -->|Data| APPEND
    APPEND -->|Loop| PARSE_LOOP
    PARSE_LOOP -->|Check| TRY_PARSE
    TRY_PARSE -->|Incomplete| RECV_CALL
    TRY_PARSE -->|Complete| HAS_COMPLETE
    HAS_COMPLETE -->|Yes| ERASE_BYTES
    ERASE_BYTES -->|Next| EXECUTE
    EXECUTE -->|Result| QUEUE_RESP
    QUEUE_RESP -->|Update| HAS_DATA
    
    HAS_DATA -->|Yes| SEND_CALL
    SEND_CALL -->|Partial| ERASE_SENT
    ERASE_SENT -->|Pending| RETRY
    RETRY -->|POLLOUT| SEND_CALL
```

## Replication Command Processing

```mermaid
graph TB
    subgraph "Master Side"
        M_RECV["Receive client cmd<br/>fd: client socket"]
        M_PARSE["Parse to RESP array"]
        M_EXEC["Execute command<br/>Update master state"]
        M_RESP["Queue +OK response<br/>to client_fd"]
        M_DETECT["Detect write command<br/>(SET, DEL, etc)"]
        M_PROP["Find replica connections<br/>is_replica_connection=true"]
        M_QUEUE["Queue RESP array<br/>in replica_fd.output_buffer"]
        M_FLUSH["Poll detects POLLOUT<br/>on replica_fd"]
        M_SEND["send() propagated command"]
    end
    
    subgraph "Replica Side"
        R_POLL["poll() on master_fd"]
        R_RECV["recv() from master_fd<br/>into input_buffer"]
        R_PARSE["try_parse_command()"]
        R_SOURCE["source=REPLICATION<br/>state=COMMAND_STREAMING"]
        R_EXEC["handleCommand()<br/>with REPLICATION source"]
        R_APPLY["RDStore.set()<br/>Apply to local data"]
        R_NO_RESP["Return nullopt<br/>No response queued"]
    end
    
    M_RECV -->|Process| M_PARSE
    M_PARSE -->|Execute| M_EXEC
    M_EXEC -->|Respond| M_RESP
    M_EXEC -->|Check| M_DETECT
    M_DETECT -->|Yes| M_PROP
    M_PROP -->|Queue| M_QUEUE
    M_QUEUE -->|Poll ready| M_FLUSH
    M_FLUSH -->|Send| M_SEND
    M_SEND -->|Arrives at| R_POLL
    
    R_POLL -->|Readable| R_RECV
    R_RECV -->|Buffer| R_PARSE
    R_PARSE -->|Parsed| R_SOURCE
    R_SOURCE -->|Execute| R_EXEC
    R_EXEC -->|Apply| R_APPLY
    R_APPLY -->|Silent| R_NO_RESP
```

## Thread Model (Still Single-Threaded)

```mermaid
graph TB
    MAIN["Main Thread<br/>Single Threaded"]

    subgraph "Initialization Phase"
        CHECK_REPLICA["Check if replica mode"]
        INIT_REPL["setup_replica_mode()"]
        HANDSHAKE["start_handshake()"]
        GET_FD["get_master_fd()"]
        SETUP_SOCK["setup_server_socket()"]
    end

    subgraph "Main Event Loop"
        BUILD_POLL["Build pollfd array<br/>from connections map"]
        CALL_POLL["poll(fds, -1)<br/>Block until event"]
        ITERATE["For each event fd"]
        CHECK_NEW["Is server_fd?"]
        CHECK_READ["Has POLLIN?"]
        CHECK_WRITE["Has POLLOUT?"]
        ACCEPT_CONN["accept_new_connection()"]
        READ_CONN["process_readable_connection()"]
        WRITE_CONN["process_writable_connection()"]
    end

    MAIN -->|Init| CHECK_REPLICA
    CHECK_REPLICA -->|Yes| INIT_REPL
    CHECK_REPLICA -->|No| SETUP_SOCK
    INIT_REPL -->|Connect| HANDSHAKE
    HANDSHAKE -->|Get fd| GET_FD
    GET_FD -->|Then| SETUP_SOCK
    SETUP_SOCK -->|Start loop| BUILD_POLL
    
    BUILD_POLL -->|Poll| CALL_POLL
    CALL_POLL -->|Events| ITERATE
    ITERATE -->|Check| CHECK_NEW
    CHECK_NEW -->|Yes| ACCEPT_CONN
    CHECK_NEW -->|No| CHECK_READ
    CHECK_READ -->|Yes| READ_CONN
    CHECK_READ -->|No| CHECK_WRITE
    CHECK_WRITE -->|Yes| WRITE_CONN
    WRITE_CONN -->|Loop| BUILD_POLL
```

## Error Handling & Connection Lifecycle

```mermaid
graph TD
    ACCEPT_FD["accept() new connection"]
    CREATE_CONN["Create Connection object<br/>state=CLIENT_COMMAND_WAITING"]
    STORE_MAP["Store in connections map"]
    
    READABLE_EVENT["POLLIN event"]
    RECV_DATA["recv() from fd"]
    RECV_ERR{recv() result}
    RECV_FAIL["recv() < 0"]
    RECV_CLOSE["recv() == 0<br/>EOF"]
    RECV_DATA_OK["recv() > 0"]
    
    APPEND_BUF["Append to<br/>input_buffer"]
    PARSE_RESP["try_parse_command()"]
    PARSE_OK["Complete RESP?"]
    EXEC_CMD["Execute command"]
    
    WRITABLE_EVENT["POLLOUT event"]
    FLUSH_BUF["Flush output_buffer"]
    SEND_DATA["send() bytes"]
    
    HANDLE_CLOSE["handle_connection_close()"]
    CLOSE_FD["close(fd)"]
    ERASE_MAP["Erase from connections"]
    
    ACCEPT_FD -->|New| CREATE_CONN
    CREATE_CONN -->|Store| STORE_MAP
    STORE_MAP -->|Wait| READABLE_EVENT
    
    READABLE_EVENT -->|Read| RECV_DATA
    RECV_DATA -->|Check| RECV_ERR
    RECV_ERR -->|Error| RECV_FAIL
    RECV_ERR -->|EOF| RECV_CLOSE
    RECV_ERR -->|Data| RECV_DATA_OK
    
    RECV_FAIL -->|Close| HANDLE_CLOSE
    RECV_CLOSE -->|Close| HANDLE_CLOSE
    RECV_DATA_OK -->|Process| APPEND_BUF
    APPEND_BUF -->|Parse| PARSE_RESP
    PARSE_RESP -->|Check| PARSE_OK
    PARSE_OK -->|Incomplete| READABLE_EVENT
    PARSE_OK -->|Complete| EXEC_CMD
    EXEC_CMD -->|Response| WRITABLE_EVENT
    
    WRITABLE_EVENT -->|Ready| FLUSH_BUF
    FLUSH_BUF -->|Send| SEND_DATA
    SEND_DATA -->|Done| READABLE_EVENT
    
    HANDLE_CLOSE -->|Cleanup| CLOSE_FD
    CLOSE_FD -->|Remove| ERASE_MAP
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
