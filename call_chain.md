# Webserv Full Request Call Chain

```
main.cpp
└── int main(int, char**)
    ├── ConfigFiles constructor          # Parse config file
    ├── Engine constructor               # Initialize engine with config
    ├── Engine::setupListeningSockets()  # Create bind/listen sockets for all server:port
    └── Engine::run()                    # Main event loop
        ├── registerListenSocketsForSelect()  # Add listen fds to select read set
        ├── registerClientSocketForSelect()   # Add client fds to select sets
        ├── select()                          # Wait for socket activity (blocks up to 1s)
        ├── checkTimeouts()                   # Close timed out connections
        ├── acceptPendingClientConnections()  # Accept new clients on listen sockets
        ├── Engine::processIncomingData() (engineIncomingData.cpp:205)  # Read from ready sockets
        │   ├── processCGIOutput() [if CGI_RUNNING]  # Read CGI pipe when ready
        │   │   ├── read() from CGI stdout pipe      # Get CGI script output
        │   │   └── buildResponseFromCGIOutput()     # Parse CGI headers + format HTTP response
        │   │       └── parseCGIHeaders()            # Extract Status, Content-Type from CGI
        │   └── handleClientSocketRead()             # Read from client socket
        │       ├── recv() from client socket        # Receive raw bytes
        │       └── Engine::handleClientRequest()    # Process received data
        │           ├── Engine::attemptIncomingHeader()  # Append to buffer, check size limits
        │           │   ├── Connection::appendToHeaderBuffer()  # Store received bytes
        │           │   └── Engine::enforceRequestSizeLimits()   # Reject if > 8KB header
        │           └── Engine::processBufferedRequests()  # Extract and process complete requests
        │               ├── Engine::handleRequestExtraction()  # Find \r\n\r\n boundary
        │               │   └── validateAndExtractRequestFromBuffer()  # Extract one full request
        │               ├── Engine::handleRequestParsing()  # Parse raw string into HttpRequest
        │               │   └── HttpRequest::parse()        # Parse method, path, headers, body
        │               ├── Engine::enforceRequestBodySizeLimit()  # Check max body size from config
        │               └── Engine::handleRequestExecution()  # Route/Execute the request
        │                   ├── findBestLocation()             # Match path to location block
        │                   ├── isMethodAllowed()              # Check if method allowed for location
        │                   ├── isCgiRequestForLocation()      # Check if path is CGI
        │                   │   └── launchCGI() [if CGI]       # Fork + exec CGI script
        │                   │       ├── fork()                 # Create child process
        │                   │       ├── execve()               # Run CGI script in child
        │                   │       └── Connection::setState(CGI_RUNNING)  # Wait for CGI
        │                   ├── Engine::handleSession() [non-CGI]  # Handle session cookies
        │                   └── routeRequest() [non-CGI]           # Handle static files/DELETE
        │                       └── appendToWriteBuffer()          # Prepare response for writing
        └── processOutgoingData()  # Send responses to clients in WRITING state
```

---

## Complete Execution Path (Normal Request):

1. **Entry Point**  
   `main.cpp:30` → `Engine::run()` (`engine.cpp:287`)

2. **Event Loop**  
   `Engine::run()` loops indefinitely until signal received  
   └── `select()` waits for activity on sockets

3. **Client Connection Accepted**  
   When new client connects:
   ```
   acceptPendingClientConnections()
   ├── accept()
   ├── fcntl(O_NONBLOCK)
   └── _clientConnections[fd] = new Connection()
   ```

4. **Incoming Data Processing**  
   When client sends data:
   ```
   processIncomingData()
   └── handleClientSocketRead()
       └── recv() → buffer
           └── handleClientRequest(conn, buffer, bytes)
               ├── attemptIncomingHeader()
               └── processBufferedRequests()
                   ├── handleRequestExtraction()
                   ├── handleRequestParsing()
                   ├── enforceRequestBodySizeLimit()
                   └── handleRequestExecution()
                       ├── If CGI → launchCGI()
                       └── Else → routeRequest()
   ```

5. **CGI Path (When executed)**
   ```
   launchCGI()
   ├── fork()
   │   ├── Child: execve() CGI script
   │   └── Parent: record pid + pipe fds
   └── conn->setState(CGI_RUNNING)

   [Next loop iteration]
   processIncomingData()
   └── processCGIOutput()
       ├── read() from CGI pipe
       └── buildResponseFromCGIOutput()
           └── conn->setState(WRITING)
   ```

6. **Response Writing**
   ```
   processOutgoingData()
   └── send() response to client
   ```

---

## Function Location Reference:
| Function | File | Line |
|----------|------|------|
| `main()` | `src/main.cpp` | 30 |
| `Engine::run()` | `src/engine/engine.cpp` | 287 |
| `processIncomingData()` | `src/engine/EngineIncomingHandling/engineIncomingData.cpp` | 205 |
| `handleClientSocketRead()` | `src/engine/EngineIncomingHandling/engineIncomingData.cpp` | 165 |
| `handleClientRequest()` | `src/engine/EngineIncomingHandling/engine_request_processing.cpp` | 315 |
| `processBufferedRequests()` | `src/engine/EngineIncomingHandling/engine_request_processing.cpp` | 238 |
| `handleRequestExecution()` | `src/engine/EngineIncomingHandling/engine_request_processing.cpp` | 156 |
| `processCGIOutput()` | `src/engine/EngineIncomingHandling/engineIncomingData.cpp` | 112 |
