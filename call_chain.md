# Webserv Full Request Call Chain (Updated)

```
main.cpp
└── int main(int, char**)
    ├── ConfigFiles::ConfigFiles()            # Parse/validate config into server objects
    ├── Engine::Engine()                      # Build runtime state (sockets, sessions, connections)
    ├── Engine::setupListeningSockets()       # bind/listen on all configured host:port pairs
    └── Engine::run()                         # Main event loop (single select gate)
        ├── registerListenSocketsForSelect()  # Add listen sockets to read set
        ├── registerClientSocketForSelect()   # Add client sockets + CGI pipes to read/write sets
        ├── select()                          # Wait until at least one descriptor is ready
        ├── checkTimeouts()                   # Enforce header/body/write/CGI timeout rules
        ├── acceptPendingClientConnections()  # Accept newly ready listen sockets
        ├── processIncomingData()             # Read from ready client sockets / CGI stdout pipes
        │   ├── processCGIOutput() [if CGI_RUNNING]
        │   │   ├── read() from CGI stdout pipe
        │   │   ├── buildResponseFromCGIOutput()
        │   │   │   └── parseCGIHeaders()
        │   │   └── setState(WRITING)
        │   └── handleClientSocketRead()
        │       ├── recv() from client socket
        │       └── handleClientRequest()
        │           ├── attemptIncomingHeader()
        │           │   ├── appendToHeaderBuffer()
        │           │   └── enforceRequestSizeLimits()
        │           └── processBufferedRequests()
        │               ├── handleRequestExtraction()
        │               │   └── validateAndExtractRequestFromBuffer()
        │               ├── handleRequestParsing()
        │               │   └── HttpRequest::parse()
        │               │       ├── parseRequestLine()
        │               │       ├── parseHeaders()
        │               │       ├── parseCookies()
        │               │       └── extractBody()
        │               └── handleRequestExecution()
        │                   ├── findBestLocation()
        │                   ├── isMethodAllowed()
        │                   ├── ensureSessionCookieHeader()
        │                   ├── launchCGI() [if CGI]
        │                   │   ├── resolveCGIScriptPath()
        │                   │   ├── validateCGIScript()
        │                   │   ├── createCGIProcess() -> fork()
        │                   │   ├── setupCGIChildProcess() -> execve()
        │                   │   └── setupCGIParent() -> setState(CGI_RUNNING)
        │                   └── routeRequest() [if non-CGI]
        │                       ├── handleGet()
        │                       ├── handlePost()
        │                       └── handleDelete()
        └── processOutgoingData()             # Write response bytes / feed CGI stdin
            ├── handleCGIStdinWrite() [if CGI_RUNNING]
            │   └── write() to CGI stdin pipe
            └── handleClientWrite()
                └── send() HTTP bytes to client
```

---

## Complete Execution Path (Continuous Request):

1. **Entry Point**  
   Input in words: command-line arguments with optional config path and process signals.  
   Output in words: initialized configuration and engine runtime objects.

2. **Socket Setup**  
   Input in words: parsed server host/port list from config.  
   Output in words: non-blocking listening sockets ready to accept clients.

3. **Event Loop Tick**  
   Input in words: current connection map, listen sockets, CGI pipe descriptors, timeout values.  
   Output in words: readiness sets from select and updated timeout decisions.

4. **Accept New Clients**  
   Input in words: readiness-marked listen descriptors.  
   Output in words: newly accepted non-blocking client sockets and `Connection` objects.

5. **Read Ready Inputs**  
   Input in words: readiness-marked client sockets and CGI stdout descriptors.  
   Output in words: client read buffers extended or CGI stdout buffers extended.

6. **Frame and Parse Requests**  
   Input in words: accumulated raw bytes in connection read buffer.  
   Output in words: one complete `HttpRequest` object (method/path/query/headers/cookies/body).

7. **Execute Request (single branch point)**  
   Input in words: parsed request + selected server/location config + connection keep-alive decision.  
   Output in words: either CGI process started or full non-CGI HTTP response string produced.

8. **Collect CGI Result (if CGI branch)**  
   Input in words: CGI stdout bytes and CGI headers/body text.  
   Output in words: normalized HTTP response bytes in write buffer.

9. **Write Outbound Data**  
   Input in words: connection write buffer and write-ready descriptors.  
   Output in words: response bytes sent, state reset to READING for keep-alive or connection closed.

---

## Function Location Reference:
| Function | File | Line |
|----------|------|------|
| `main()` | `src/main.cpp` | 20 |
| `Engine::run()` | `src/engine/engine.cpp` | 255 |
| `Engine::registerListenSocketsForSelect()` | `src/engine/engine.cpp` | 88 |
| `Engine::registerClientSocketForSelect()` | `src/engine/engine.cpp` | 103 |
| `Engine::checkTimeouts()` | `src/engine/engine.cpp` | 182 |
| `Engine::acceptPendingClientConnections()` | `src/engine/engine.cpp` | 140 |
| `Engine::processIncomingData()` | `src/engine/EngineIncomingHandling/engineIncomingData.cpp` | 168 |
| `Engine::processCGIOutput()` | `src/engine/EngineIncomingHandling/engineIncomingData.cpp` | 101 |
| `Engine::buildResponseFromCGIOutput()` | `src/engine/EngineIncomingHandling/engineIncomingData.cpp` | 45 |
| `Engine::parseCGIHeaders()` | `src/engine/EngineIncomingHandling/engineIncomingData.cpp` | 15 |
| `Engine::handleClientSocketRead()` | `src/engine/EngineIncomingHandling/engineIncomingData.cpp` | 141 |
| `Engine::handleClientRequest()` | `src/engine/EngineIncomingHandling/engine_request_processing.cpp` | 319 |
| `Engine::attemptIncomingHeader()` | `src/engine/EngineIncomingHandling/engine_request_processing.cpp` | 229 |
| `Engine::processBufferedRequests()` | `src/engine/EngineIncomingHandling/engine_request_processing.cpp` | 249 |
| `Engine::handleRequestExtraction()` | `src/engine/EngineIncomingHandling/engine_request_processing.cpp` | 102 |
| `validateAndExtractRequestFromBuffer()` | `src/httpHandling/requestValidator.cpp` | 181 |
| `Engine::handleRequestParsing()` | `src/engine/EngineIncomingHandling/engine_request_processing.cpp` | 134 |
| `HttpRequest::parse()` | `src/httpHandling/httpRequest_parse.cpp` | 222 |
| `Engine::handleRequestExecution()` | `src/engine/EngineIncomingHandling/engine_request_processing.cpp` | 164 |
| `Engine::findBestLocation()` | `src/engine/EngineIncomingHandling/engine_routing.cpp` | 28 |
| `Engine::isMethodAllowed()` | `src/engine/EngineIncomingHandling/engine_routing.cpp` | 52 |
| `Engine::ensureSessionCookieHeader()` | `src/engine/EngineIncomingHandling/engine_request_processing.cpp` | 69 |
| `Engine::launchCGI()` | `src/engine/engine_cgi.cpp` | 320 |
| `Engine::routeRequest()` | `src/engine/EngineIncomingHandling/engine_routing.cpp` | 197 |
| `Engine::processOutgoingData()` | `src/engine/engineOutgoingData.cpp` | 102 |
| `Engine::handleCGIStdinWrite()` | `src/engine/engineOutgoingData.cpp` | 15 |
| `Engine::handleClientWrite()` | `src/engine/engineOutgoingData.cpp` | 49 |

---

## Function Roles With Input/Output (in words)
| Function | Input in words | Output in words | What it does briefly |
|----------|----------------|-----------------|----------------------|
| `Engine::handleRequestExtraction()` | Raw accumulated bytes in one connection read buffer | One full raw HTTP request string when complete | Frames one request safely from a potentially partial stream. |
| `validateAndExtractRequestFromBuffer()` | Mutable buffer text that may contain headers and body | Extracted request text and remaining unread bytes in buffer | Detects request boundary for content-length or chunked bodies. |
| `Engine::handleRequestParsing()` | Raw request text and configured max body size | Structured request object or parse error response | Converts text protocol data into typed request fields. |
| `HttpRequest::parse()` | One full raw request string | Parsed method/path/query/headers/cookies/body fields | Performs request-line, header, cookie, and body parsing. |
| `Engine::handleRequestExecution()` | Parsed request and selected server/location config | Final response bytes or CGI process startup | Applies rules, session cookie policy, and dispatches CGI/non-CGI paths. |
| `Engine::launchCGI()` | Request context and CGI route config | Connection enters CGI_RUNNING or immediate error response | Creates CGI process and pipe wiring for stdin/stdout. |
| `Engine::routeRequest()` | Parsed non-CGI request and route config | Complete HTTP response string | Dispatches GET, POST, DELETE handling and route-level behavior. |
| `Engine::processOutgoingData()` | Write-ready descriptors and connection write buffers | Sent bytes and updated connection states | Drains response buffers and finalizes keep-alive/close transitions. |
