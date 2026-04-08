# Webserv Request Call Chain - Comprehensive Architecture

## Overview

This document describes the complete execution flow of an HTTP request through the webserv HTTP server, from server startup through request handling to response delivery.

---

## Phase 1: Server Initialization

```
main.cpp
└── int main(int argc, char **argv)
    ├── signal(SIGPIPE, SIG_IGN)                    // Ignore SIGPIPE to prevent crashes on client disconnect
    ├── std::srand()                                // Seed random number generator
    ├── ConfigFiles::ConfigFiles(argv[1])           // Load configuration
    │   ├── ConfigFiles::loadFromFile(path)         // Parse config file
    │   │   ├── tokenizeConfig()                    // Lex config tokens
    │   │   ├── parseServerBlock()                  // Parse server block
    │   │   └── parseLocationBlock()                // Parse location block
    │   └── ConfigFiles::initDummy()                // Fallback dummy config
    ├── Engine::Engine(config)                      // Initialize engine with config
    │   └── Initialize connection map, session map
    ├── Engine::setupListeningSockets()              // Create listening sockets
    │   ├── createListeningSocket()                 // Create socket, bind, listen
    │   └── Store fd -> host:port mapping
    └── Engine::run()                                // Enter main event loop
```

---

## Phase 2: Main Event Loop

```
Engine::run()
├── signal(SIGINT/SIGTERM, handler)                // Setup signal handlers
└── while (!g_engineStopRequested)
    ├── fd_set readSet, writeSet                    // Initialize fd sets
    ├── registerListenSocketsForSelect()            // Add listen sockets to readSet
    ├── registerClientSocketForSelect()             // Add client sockets/CGI pipes
    │   ├── READING state -> add to readSet
    │   ├── WRITING state -> add to writeSet
    │   └── CGI_RUNNING -> add CGI stdout/stdin
    ├── select(maxFd+1, readSet, writeSet, timeout) // Wait for I/O readiness
    ├── checkTimeouts()                             // Enforce timeouts
    │   ├── header timeout (5s)
    │   ├── body timeout (30s)
    │   ├── write timeout (60s)
    │   └── CGI timeout (10s)
    ├── acceptPendingClientConnections(readSet)     // Accept new clients
    │   ├── accept() on ready listen sockets
    │   ├── fcntl(O_NONBLOCK)
    │   └── Create Connection object
    ├── processIncomingData(readSet)                // Handle incoming data
    │   └── See Phase 3
    ├── processOutgoingData(writeSet)               // Handle outgoing data
    │   └── See Phase 5
    └── waitpid(-1, NULL, WNOHANG)                  // Reap finished CGI processes
```

---

## Phase 3: Incoming Data Processing

```
Engine::processIncomingData(readSet)
└── For each client connection
    ├── if state == CGI_RUNNING
    │   └── processCGIOutput()                     // Read CGI output
    │       ├── read() from CGI stdout pipe
    │       ├── append to cgi->stdout_buffer
    │       └── if EOF -> buildResponseFromCGIOutput()
    │           ├── parseCGIHeaders()               // Parse CGI status/content-type
    │           ├── buildResponse()                // Build HTTP response
    │           └── setState(WRITING)
    └── else (READING state)
        └── handleClientSocketRead()
            ├── recv() from client socket
            └── handleClientRequest()
                ├── attemptIncomingHeader()         // Read and buffer headers
                │   ├── appendToHeaderBuffer()
                │   ├── find("\r\n\r\n")            // Look for header end
                │   └── enforceRequestSizeLimits()  // Check size limits
                └── processBufferedRequests()       // Extract and parse requests
                    ├── handleRequestExtraction()    // Frame complete request
                    │   └── validateAndExtractRequestFromBuffer()
                    │       ├── parse content-length
                    │       └── parse chunked encoding
                    ├── handleRequestParsing()       // Parse HTTP request
                    │   └── HttpRequest::parse()
                    │       ├── parseRequestLine()   // METHOD /path HTTP/1.1
                    │       ├── parseHeaders()       // Key: value pairs
                    │       ├── parseCookies()       // Cookie: header
                    │       └── extractBody()        // Read body based on encoding
                    └── handleRequestExecution()     // Execute request
                        ├── findBestLocation()       // Match URI to location
                        ├── isMethodAllowed()        // Check allowed methods
                        ├── ensureSessionCookieHeader() // Manage sessions
                        └── Branch: CGI or non-CGI
```

---

## Phase 4: Request Execution (Two Paths)

### Path 4A: CGI Execution

```
Engine::handleRequestExecution()
└── launchCGI()
    ├── resolveCGIScriptPath()                    // Build script filesystem path
    │   └── mapRequestPathForLocationRoot()
    ├── validateCGIScript()                        // Verify script exists/executable
    │   └── FileHandler::fileExists()
    ├── createCGIProcess()                        // Create pipes and fork
    │   ├── pipe(in_pipe)                          // stdin pipe
    │   ├── pipe(out_pipe)                         // stdout pipe
    │   └── fork()                                // Create child process
    ├── if pid == 0 (child process)
    │   └── setupCGIChildProcess()
    │       ├── dup2 stdin/stdout                  // Redirect stdio
    │       ├── chdir to script directory
    │       ├── build environment variables
    │       │   ├── REQUEST_METHOD
    │       │   ├── REQUEST_URI
    │       │   ├── QUERY_STRING
    │       │   ├── CONTENT_LENGTH
    │       │   ├── CONTENT_TYPE
    │       │   ├── HTTP_* headers
    │       │   └── SERVER_* variables
    │       └── execve()                          // Execute CGI script
    └── else (parent process)
        └── setupCGIParent()
            ├── close unused pipe ends
            ├── create CGIContext
            ├── set non-blocking on pipes
            └── setState(CGI_RUNNING)
```

### Path 4B: Direct HTTP Handling

```
Engine::handleRequestExecution()
└── routeRequest()
    ├── if location has return directive
    │   └── buildRedirectResponse()
    ├── if method == GET
    │   └── handleGet()
    │       ├── hasPathTraversal()                // Security check
    │       ├── resolveLocationRoot()              // Get document root
    │       ├── mapRequestPathForLocationRoot()    // Apply location path mapping
    │       ├── FileHandler::resolvePath()         // Resolve index file
    │       ├── if directory -> check index or autoindex
    │       ├── FileHandler::readFile()            // Read file content
    │       └── FileHandler::getMimeType()         // Determine MIME type
    ├── if method == POST
    │   └── handlePost()
    │       ├── isUploadEnabled()                 // Check upload config
    │       ├── extractUploadFilename()           // Parse Content-Disposition
    │       ├── ensureDirectoryExists()           // Create upload dir
    │       └── open() + write()                  // Save uploaded file
    └── if method == DELETE
        └── handleDelete()
            ├── FileHandler::fileExists()         // Check file exists
            └── std::remove()                     // Delete file
```

---

## Phase 5: Outgoing Data Processing

```
Engine::processOutgoingData(writeSet)
└── For each client connection
    ├── if state == CGI_RUNNING
    │   └── handleCGIStdinWrite()                 // Feed body to CGI stdin
    │       ├── write() to CGI stdin pipe
    │       └── close stdin when complete
    └── handleClientWrite()                       // Send response to client
        ├── if writeBuffer empty
        │   ├── if shouldClose -> destroy connection
        │   └── else -> reset to READING (keep-alive)
        └── else
            └── send() to client socket
                ├── if complete -> reset or close
                └── if partial -> continue next iteration
```

---

## Response Building

```
Engine::buildErrorResponse(statusCode, shouldClose, serverConfig)
├── Lookup error page from server config
├── If not found -> generate default HTML
└── buildStandardResponse()

Engine::buildStandardResponse(statusCode, body, contentType, shouldClose)
├── Build status line (HTTP/1.1 200 OK)
├── Add Content-Type header
├── Add Content-Length header
├── Add Connection header (close/keep-alive)
└── Return complete HTTP response string
```

---

## Data Structures

### Connection
- `fd` - Client socket file descriptor
- `state` - READING, WRITING, CGI_RUNNING
- `requestState` - READING_HEADERS, READING_BODY, COMPLETE
- `readBuffer` - Raw request data
- `writeBuffer` - Response data
- `cgiContext` - CGI process state (pid, pipes, buffers)
- `serverConfig` - Associated server configuration

### HttpRequest
- `method` - GET, POST, DELETE
- `path` - Request URI path
- `query` - Query string
- `version` - HTTP/1.1
- `headers` - Header key-value map
- `cookies` - Parsed cookies
- `body` - Request body

### ServerConfig
- `host`, `port` - Listening address
- `root` - Document root
- `locations` - Location block configs

---

## Key File Locations

| Component | File | Purpose |
|-----------|------|---------|
| Entry point | `src/main.cpp` | Server startup |
| Event loop | `src/engine/engine.cpp` | Main select loop |
| Request read | `src/engine/EngineIncomingHandling/engineIncomingData.cpp` | Socket reading |
| Request parse | `src/engine/EngineIncomingHandling/engine_request_processing.cpp` | HTTP parsing |
| Routing | `src/engine/EngineIncomingHandling/engine_routing.cpp` | GET/POST/DELETE handling |
| CGI | `src/engine/engine_cgi.cpp` | CGI process management |
| Response | `src/engine/engine_response.cpp` | Response building |
| Write | `src/engine/engineOutgoingData.cpp` | Sending response |
| Config | `src/config/dummyConfigFiles.cpp` | Config parsing |
| HTTP parsing | `src/httpHandling/httpRequest_parse.cpp` | Request parsing |
| Validation | `src/httpHandling/requestValidator.cpp` | Request validation |

---

## State Machine

```
                    ┌─────────────┐
                    │  READING    │
                    └──────┬──────┘
                           │ recv() complete request
                           ▼
                    ┌─────────────┐
        ┌────────── │  COMPLETE   │ ───────────┐
        │          └──────┬──────┘            │
        │                 │                   │
        ▼                 ▼                   ▼
   ┌─────────┐      ┌──────────┐        ┌──────────┐
   │ CGI     │      │ Non-CGI  │        │  Error   │
   │ RUNNING │      │ Response │        │ Response │
   └────┬────┘      └────┬─────┘        └────┬─────┘
        │                │                    │
        │ CGI output     │ send() complete    │ send() complete
        ▼                ▼                    ▼
   ┌─────────┐      ┌──────────┐        ┌──────────┐
   │ WRITING │      │ WRITING  │        │ WRITING  │
   └────┬────┘      └────┬─────┘        └────┬─────┘
        │                │                    │
        │ send complete  │ send complete      │ send complete
        ▼                ▼                    ▼
   ┌─────────┐      ┌──────────┐        ┌──────────┐
   │ should- │      │ keep-    │        │ should-  │
   │ close?  │      │ alive?   │        │ close?   │
   └────┬────┘      └────┬─────┘        └────┬─────┘
        │                │                    │
        ▼                ▼                    ▼
   Close/              Back to            Close/
   Destroy             READING             Destroy
```

---

## Timeout Rules

| State | Timeout | Action |
|-------|---------|--------|
| READING_HEADERS | 5s | Send 408 Request Timeout |
| READING_BODY | 30s | Send 408 Request Timeout |
| WRITING | 60s | Close connection |
| CGI_RUNNING | 10s | Kill CGI, send 504 Gateway Timeout |

---

## Session Management

```
ensureSessionCookieHeader(request)
├── Check for existing "webservsid" cookie
├── If valid -> update expiration
├── If invalid -> generate new session ID
├── Store in _sessions map (sessionId -> expirationTime)
└── Return Set-Cookie header string
```

---

## Complete Request Flow Timeline

1. **Socket Accept**: Listen socket becomes readable → accept() creates client connection
2. **Header Read**: recv() → append to read buffer → find \r\n\r\n
3. **Request Extraction**: validateAndExtractRequestFromBuffer() → extract complete request
4. **Request Parsing**: HttpRequest::parse() → method, path, headers, body
5. **Route Matching**: findBestLocation() → match URI to location block
6. **Method Check**: isMethodAllowed() → verify method permitted
7. **CGI Check**: isCgiRequestForLocation() → determine CGI vs direct
8. **Execution**: Either launchCGI() or routeRequest()
9. **Response Build**: buildStandardResponse() → HTTP response string
10. **Send Response**: send() to client socket
11. **Keep-alive**: Reset to READING or close based on Connection header