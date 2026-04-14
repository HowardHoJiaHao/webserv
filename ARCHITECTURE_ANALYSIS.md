# Webserver Architecture Analysis

## Overview
This is a C++ HTTP webserver project with two main implementations:
1. **Active Implementation** (kaydooo/) - A feature-rich webserver with CGI support, config parsing, and multi-client handling
2. **Development Version** (include/ & src/) - A simpler implementation being developed

This analysis focuses on the active **kaydooo** implementation, which is more complete and production-ready.

---

## High-Level Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     Application Entry Point                  │
│                      (main.cpp)                              │
└────────────────┬────────────────────────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────────────────────────────┐
│             Configuration Layer                              │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ ConfigParser → ServerConfig → Location               │   │
│  │ (Parses config files)                                │   │
│  └──────────────────────────────────────────────────────┘   │
└────────────────┬────────────────────────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────────────────────────────┐
│             Server Management Layer                          │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ ServerManager                                        │   │
│  │ - Socket setup and multiplexing (select())          │   │
│  │ - Connection management                             │   │
│  │ - Request/Response orchestration                    │   │
│  └──────────────────────────────────────────────────────┘   │
└────────────────┬────────────────────────────────────────────┘
                 │
        ┌────────┴────────┐
        ▼                 ▼
   ┌─────────────┐  ┌──────────────┐
   │   Client    │  │Client Manager│
   │  (Request/  │  │ (Per-client  │
   │ Response)   │  │   state)     │
   └─────────────┘  └──────────────┘
        │
        ├─────────────────────────────┐
        ▼                             ▼
   ┌──────────────┐           ┌─────────────────┐
   │ HttpRequest  │           │   Response      │
   │ (Parse HTTP) │           │ (Build HTTP)    │
   └──────────────┘           │ + CGI Handler   │
                              └─────────────────┘
                                    │
                                    ▼
                              ┌──────────────────┐
                              │  CgiHandler      │
                              │ (Execute scripts)│
                              └──────────────────┘
```

---

## Core Components

### 1. **Entry Point: `main.cpp`**
- **Responsibility**: Application startup and initialization
- **Key Operations**:
  - Signal handling (SIGPIPE)
  - Config file loading (default: `configs/default.conf`)
  - Initializes ServerManager and ConfigParser
  - Exception handling wrapper

**Key Code**:
```cpp
ConfigParser cluster;
ServerManager master;
cluster.createCluster(config);
master.setupServers(cluster.getServers());
master.runServers();
```

---

### 2. **Configuration Layer**

#### **ConfigParser**
- **Purpose**: Parses HTTP server configuration files
- **Responsibilities**:
  - Parse config file syntax
  - Create ServerConfig objects
  - Validate configuration
  - Expose `createCluster()` and `getServers()`
- **Output**: `std::vector<ServerConfig>`

#### **ServerConfig**
- **Purpose**: Represents a single virtual server
- **Contains**:
  - Host and port
  - Server name(s)
  - Root directory
  - Default files (index.html)
  - Error pages map
  - Vector of Location configs
  - Client max body size limit
  - HTTP method restrictions
- **Key Methods**:
  - `getLocations()` - Access routing rules

#### **Location**
- **Purpose**: Route matching and directive storage
- **Contains**:
  - Path pattern (e.g., `/api`)
  - Redirect URI
  - Root directory override
  - Default files
  - CGI extension associations
  - HTTP method permissions
  - Upload directory
  - Auto-index flag

---

### 3. **Server Management Layer**

#### **ServerManager** (Main event loop orchestrator)
- **Responsibility**: Core server operations and socket multiplexing
- **Key Responsibilities**:
  1. **Server Setup** (`setupServers`):
     - Create listening sockets for each server config
     - Bind to host:port combinations
     - Store socket → ServerConfig mappings

  2. **Event Loop** (`runServers`):
     - Use `select()` for I/O multiplexing
     - Monitor listening sockets for new connections
     - Monitor client sockets for read/write readiness
     - Handle timeouts (CONNECTION_TIMEOUT = 60 seconds)

  3. **Connection Lifecycle**:
     ```
     acceptNewConnection()
           ↓
     readRequest() → handleReqBody()
           ↓
     assignServer() (find matching ServerConfig)
           ↓
     sendResponse() / sendCgiBody()
           ↓
     closeConnection()
     ```

- **Key Methods**:
  - `acceptNewConnection()` - Accept new clients
  - `readRequest()` - Receive HTTP request from client
  - `handleReqBody()` - Process request body (POST data)
  - `sendResponse()` - Send HTTP response
  - `sendCgiBody()` / `readCgiResponse()` - CGI interaction
  - `checkTimeout()` - Kick out idle clients
  - `initializeSets()` / `addToSet()` / `removeFromSet()` - FD set management

- **State Management**:
  - `_servers_map`: listening_socket_fd → ServerConfig
  - `_clients_map`: client_socket_fd → Client
  - `_recv_fd_pool`: Set of sockets ready to read
  - `_write_fd_pool`: Set of sockets ready to write

---

### 4. **Client & Request/Response Layer**

#### **Client**
- **Purpose**: Per-connection state container
- **Stores**:
  - Socket file descriptor
  - Client address (sockaddr_in)
  - HttpRequest object
  - Response object
  - Associated ServerConfig
  - Last activity timestamp
- **Key Methods**:
  - `setSocket()`, `setAddress()`, `setServer()`
  - `buildResponse()` - Trigger response generation
  - `updateTime()` - Update activity timestamp for timeout tracking
  - `clearClient()` - Reset for connection reuse (keep-alive)

#### **HttpRequest** (State Machine Parser)
- **Purpose**: Parse incoming HTTP request bytes using a state machine
- **Parsing States** (44 states total):
  - Request-line parsing (method, URI, HTTP version)
  - Header parsing (name-value pairs)
  - Body parsing (chunked or content-length)
  - Fragment support
  - Query string extraction

- **Key Properties**:
  - `_method`: GET, POST, DELETE, PUT, HEAD
  - `_path`: URI path
  - `_query`: Query string
  - `_fragment`: URI fragment
  - `_request_headers`: Map of header name → value
  - `_body`: Request body (as vector<uint8_t>)
  - `_boundary`: Multipart form boundary

- **Key Methods**:
  - `feed(char* data, size_t size)` - Feed request bytes incrementally
  - `parsingCompleted()` - Check if parsing finished
  - `errorCode()` - Return HTTP error if invalid
  - `keepAlive()` - Check Connection: keep-alive header
  - `getHeader(name)` - Retrieve header value
  - `getMethodStr()` - Convert method enum to string

#### **Response** (Response Builder)
- **Purpose**: Build and manage HTTP response
- **Response Building Process**:
  1. **findTarget()** - Match request path to file/directory
  2. **checkLocationPermissions()** - Verify method allowed
  3. **buildBody()** - Prepare response body
  4. **setStatusLine()** - HTTP status (200, 404, 500, etc.)
  5. **setHeaders()** - Content-Type, Content-Length, etc.
  6. **Assembly** - Combine into final response string

- **Special Features**:
  - **Error Handling**: Serve custom error pages from config
  - **Directory Listing**: Auto-index if enabled
  - **File Serving**: Read files with size limits
  - **CGI Processing**: Delegate to CgiHandler
  - **Chunked Encoding**: Support for large responses
  - **Multipart Support**: Parse multipart form boundaries

- **Key Methods**:
  - `buildResponse()` - Main response building
  - `buildBody()` - Prepare response payload
  - `setStatusLine()` / `setHeaders()`
  - `handleCgi()` - CGI script execution
  - `setErrorResponse(code)` - Error page generation
  - `getRes()` / `getLen()` - Retrieve built response
  - `cutRes(size)` - Support for partial sends

---

### 5. **CGI Layer**

#### **CgiHandler** (Child Process Management)
- **Purpose**: Execute CGI scripts (Python, Bash, etc.)
- **CGI Lifecycle**:
  1. **initEnv()** - Build CGI environment variables
  2. **execute()** - Fork child process and exec script
  3. **sendHeaderBody()** - Send POST/PUT data to script stdin
  4. **readCgiResponse()** - Read script output from stdout

- **Key Operations**:
  - **Environment Setup**:
    - REQUEST_METHOD (GET, POST, etc.)
    - QUERY_STRING (from URL)
    - CONTENT_TYPE (for POST)
    - CONTENT_LENGTH (body size)
    - SERVER_NAME, SERVER_PORT
    - SCRIPT_NAME, PATH_INFO
    - HTTP_* (for all HTTP headers)

  - **Process Management**:
    - Create pipes: stdin ← script → stdout
    - Fork child process
    - Use `execve()` to run script
    - Read response from child

  - **Response Parsing**:
    - Separate headers from body
    - Handle custom headers set by script
    - Support for Set-Cookie directives

- **Key Methods**:
  - `initEnv()` - Populate environment map
  - `execute()` - Fork and run CGI script
  - `sendHeaderBody()` - Send request body to script
  - `fixHeader()` - Parse CGI output headers
  - `setCookie()` - Handle Set-Cookie headers
  - `getPathInfo()` - Extract PATH_INFO from request

---

### 6. **Utility Components**

#### **Mime** (MIME Type Detection)
- Maps file extensions to Content-Type headers
- Used by Response to set correct Content-Type

#### **Logger** (Debugging/Logging)
- Supports different output levels
- Can log to console or file
- Configurable via `Logger::setState()`

#### **Utils** (Helper Functions)
- `statusCodeString()` - Convert code to "200 OK", etc.
- `getErrorPage()` - Retrieve error page HTML
- `buildHtmlIndex()` - Generate directory listing HTML
- `ft_stoi()` - String to integer conversion
- `fromHexToDec()` - Hex string to decimal (for chunk sizes)

---

## Data Flow Diagrams

### Request Processing Flow

```
[Client sends HTTP request]
        ↓
[ServerManager.select() detects readable socket]
        ↓
[readRequest() feeds bytes to HttpRequest state machine]
        ↓
[HttpRequest.parsingCompleted() == true]
        ↓
[handleReqBody() buffers POST/PUT data]
        ↓
[assignServer() finds matching ServerConfig]
        ↓
[Response.buildResponse()]
    ├─→ Find target file/directory
    ├─→ Check Location permissions
    ├─→ Decide: static file, directory, or CGI?
    │
    ├─→ [STATIC FILE]
    │   └─→ Read file content
    │       └─→ Set 200 + Content-Type + body
    │
    ├─→ [DIRECTORY]
    │   ├─→ Look for index files
    │   ├─→ Or generate directory listing (if auto-index on)
    │   └─→ Set 200 or 404
    │
    └─→ [CGI SCRIPT]
        └─→ CgiHandler.execute()
            ├─→ Create environment
            ├─→ Fork process
            ├─→ Exec script with request data
            ├─→ Read script output
            └─→ Parse headers + body
        ↓
[ServerManager detects writable socket]
        ↓
[sendResponse() / sendCgiBody() sends response bytes]
        ↓
[Response fully sent OR incomplete, retry next loop]
        ↓
[Keep-Alive? Clear client state : Close connection]
```

### CGI Execution Flow

```
[Response.handleCgi()] 
        ↓
[CgiHandler initialized with script path]
        ↓
[initEnv() builds environment map]
        ↓
[execute()]
    ├─→ Pipe creation (pipe_in, pipe_out)
    ├─→ fork()
    │   ├─→ [Child process]
    │   │   ├─→ dup2() pipes to stdin/stdout
    │   │   ├─→ execve(script_path, args, env)
    │   │   └─→ (script runs with CGI environment)
    │   │
    │   └─→ [Parent process]
    │       ├─→ sendHeaderBody() writes request to pipe_in
    │       ├─→ readCgiResponse() reads from pipe_out
    │       ├─→ fixHeader() parses output headers
    │       └─→ Returns response to ServerManager
```

---

## Key Design Patterns

### 1. **State Machine Pattern** (HttpRequest)
- Incremental parsing of HTTP requests
- 44 defined parsing states
- Handles incomplete/invalid requests gracefully
- Efficient streaming processing

### 2. **Manager Pattern** (ServerManager)
- Centralized orchestration of multiple servers
- Multiplexed I/O using select()
- Lifecycle management of clients

### 3. **Resource Container Pattern** (Client)
- Encapsulates all per-connection state
- Clean abstraction for client management

### 4. **Builder Pattern** (Response)
- Incremental construction of HTTP response
- Separation of status, headers, and body
- Support for different response types (static, CGI, error)

### 5. **Adapter Pattern** (CgiHandler)
- Bridges HTTP requests to CGI protocol
- Handles process spawning and I/O redirection

---

## Configuration Hierarchy

```
config file (e.g., default.conf)
    ↓
ConfigParser.createCluster()
    ↓
std::vector<ServerConfig>
    ├─ ServerConfig[0]
    │   ├─ listen: 127.0.0.1:8080
    │   ├─ server_name: "example.com"
    │   ├─ root: ./docs
    │   ├─ index: index.html
    │   └─ Location[] (routing rules)
    │       ├─ Location[0] → /api → CGI (.py, .sh)
    │       ├─ Location[1] → /upload → POST allowed
    │       └─ Location[2] → / → static files
    │
    └─ ServerConfig[1]
        ├─ listen: 0.0.0.0:9000
        └─ ...
```

---

## Key Constants & Limits

```cpp
CONNECTION_TIMEOUT      = 60 seconds     // Idle connection timeout
MESSAGE_BUFFER          = 40000 bytes    // Read buffer size per recv()
MAX_URI_LENGTH          = 4096 bytes     // Max URL length
MAX_CONTENT_LENGTH      = 30000000 bytes // Max POST body (30 MB)
```

---

## Socket & FD Management

### File Descriptor Organization
```
_servers_map
├─ FD 3 → ServerConfig (listen on :8080)
├─ FD 4 → ServerConfig (listen on :9000)
└─ FD 5 → ServerConfig (listen on :8081)

_clients_map
├─ FD 6 → Client (HTTP request from 192.168.1.100)
├─ FD 7 → Client (Keep-alive from 192.168.1.101)
├─ FD 8 → Client (CGI in progress)
└─ FD 9 → Client (Writing response)

_recv_fd_pool (fd_set)
├─ FD 3 (listening)
├─ FD 4 (listening)
├─ FD 6 (client readable)
└─ FD 7 (client readable)

_write_fd_pool (fd_set)
├─ FD 8 (client writable)
└─ FD 9 (client writable)
```

---

## Connection Lifecycle

```
1. ACCEPT PHASE
   └─ ServerManager detects listening socket readable
   └─ acceptNewConnection() creates new Client
   └─ Add client FD to _recv_fd_pool

2. READ PHASE
   └─ ServerManager.select() detects client readable
   └─ readRequest() calls HttpRequest.feed()
   └─ State machine processes bytes
   └─ When complete: move to RESPONSE phase

3. BODY PHASE (for POST/PUT)
   └─ handleReqBody() buffers additional data
   └─ Respects MAX_CONTENT_LENGTH limit
   └─ Continue accumulating until Content-Length reached

4. RESPONSE PHASE
   └─ assignServer() finds matching ServerConfig
   └─ Response.buildResponse() generates response
   └─ Add client FD to _write_fd_pool

5. WRITE PHASE
   └─ ServerManager.select() detects client writable
   └─ sendResponse() / sendCgiBody() sends bytes
   └─ Track partial writes (responses > 1 send() call)
   └─ When complete: decide Keep-Alive or close

6. CLOSE/REUSE PHASE
   └─ If Keep-Alive: clearClient(), back to step 1
   └─ If no Keep-Alive: closeConnection()
   └─ Remove from maps and FD sets

7. TIMEOUT PHASE (ongoing)
   └─ checkTimeout() runs every event loop iteration
   └─ If (now - last_activity) > CONNECTION_TIMEOUT
   └─ Force closeConnection()
```

---

## Request Routing Logic

```
Client sends:  GET /api/users?id=5 HTTP/1.1
               Host: example.com

Resolution:
1. assignServer()
   └─ Find ServerConfig matching "example.com"
   └─ If Host header not found: use first server

2. Response.buildResponse()
   ├─ Extract path: /api/users
   ├─ Loop through Location[]
   │   └─ Try to match /api/users against Location path patterns
   │
   ├─ [Match: Location /api]
   │   ├─ Check if method GET is allowed
   │   ├─ Check if /api is CGI location
   │   ├─ Find .py extension handler
   │   └─ Execute CGI script
   │
   └─ [No match or not CGI]
       ├─ Use Location root directory
       ├─ Resolve: root + /api/users
       ├─ If file: serve it
       ├─ If directory: serve index or list
       └─ If not found: 404 error page
```

---

## Error Handling Strategy

1. **Parsing Errors**: HttpRequest.errorCode() returns error status
2. **Permission Errors**: Response checks Location.allowed_methods
3. **File Not Found**: Serve configured 404.html
4. **Server Errors**: Try error page, fallback to built-in HTML
5. **CGI Errors**: Capture script exit code, serve error page
6. **Timeout**: Close connection, free resources
7. **Exceptions**: Caught in main(), logged to stderr

---

## Performance Considerations

### Efficiency Features
1. **Multiplexed I/O**: select() vs threads/forks
   - Single-threaded event loop
   - No thread overhead
   - Scales to ~1000 connections (fd_set limit)

2. **Incremental Request Parsing**
   - State machine processes bytes as they arrive
   - No buffering entire request before processing
   - Early error detection

3. **Partial Response Sends**
   - Response can be sent in multiple write() calls
   - Large files streamed without full buffering
   - `cutRes()` manages partially-sent responses

4. **Connection Reuse**
   - Keep-Alive support
   - `clearClient()` resets state for next request
   - Avoids connection setup overhead

### Potential Bottlenecks
1. Single-threaded: CPU-bound CGI blocks event loop
2. CGI execution: fork() overhead per request
3. File I/O: Entire file read into memory for static serving
4. Buffer limits: MAX_CONTENT_LENGTH = 30 MB

---

## File Organization

```
mcp/kaydooo/
├── src/
│   ├── main.cpp              (Entry point)
│   ├── ServerManager.cpp     (Event loop orchestration)
│   ├── Client.cpp            (Per-connection state)
│   ├── HttpRequest.cpp       (HTTP parsing state machine)
│   ├── Response.cpp          (Response building)
│   ├── CgiHandler.cpp        (CGI execution)
│   ├── ConfigParser.cpp      (Config file parsing)
│   ├── ServerConfig.cpp      (Server configuration)
│   ├── Location.cpp          (Route configuration)
│   ├── Logger.cpp            (Debugging)
│   ├── Mime.cpp              (MIME types)
│   ├── Utils.cpp             (Utility functions)
│   └── ConfigFile.cpp        (Config file I/O)
│
├── inc/
│   ├── Webserv.hpp           (Main includes/constants)
│   ├── ServerManager.hpp
│   ├── Client.hpp
│   ├── HttpRequest.hpp
│   ├── Response.hpp
│   ├── CgiHandler.hpp
│   ├── ConfigParser.hpp
│   ├── ServerConfig.hpp
│   ├── Location.hpp
│   ├── Logger.hpp
│   ├── Mime.hpp
│   └── ...
│
├── configs/
│   ├── default.conf          (Main config)
│   └── siege.conf            (Stress test config)
│
├── cgi-bin/                  (CGI scripts)
│   ├── *.py, *.sh            (Python/Bash scripts)
│   └── sessions/             (Session storage)
│
└── docs/
    └── fusion_web/           (Static website files)
        ├── *.html
        ├── assets/
        │   ├── css/
        │   ├── images/
        │   └── js/
        └── error_pages/      (Error page templates)
```

---

## Summary

This webserver implements a **production-oriented HTTP server** using:
- **Event-driven I/O** (select multiplexing)
- **State machine parsing** (for HTTP requests)
- **Multi-server support** (virtual hosting)
- **CGI support** (dynamic content)
- **Clean separation of concerns** (Config → Manager → Client → Request/Response)
- **Resource management** (timeouts, limits, error handling)

The architecture prioritizes **simplicity, efficiency, and correctness** while maintaining support for HTTP/1.1 features like Keep-Alive, chunked encoding, and multipart forms.
