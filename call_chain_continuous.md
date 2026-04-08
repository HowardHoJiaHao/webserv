# Continuous Request Call Chain (Copilot Version)

This is one continuous call chain from process start until a full HTTP response is sent, with a single branch point for CGI versus non-CGI handling.

| Step | Function | Input in words | Output in words | Brief role |
|---|---|---|---|---|
| 1 | main | Command-line arguments and process environment | Exit code and process lifetime control | Program entrypoint that wires config and engine startup. |
| 2 | ConfigFiles::ConfigFiles | Optional config file path string | Parsed server configuration objects in memory | Loads and validates server, location, limits, error pages, and route directives. |
| 3 | Engine::Engine | Parsed config object | Initialized engine state and runtime containers | Prepares socket maps, connection map, and session state. |
| 4 | Engine::setupListeningSockets | Server host and port pairs from config | Bound and listening non-blocking socket descriptors | Creates listening endpoints for all configured interfaces and ports. |
| 5 | Engine::run | Engine state with listening sockets | Continuous event-loop processing until stop signal | Drives all runtime I/O using select readiness sets. |
| 6 | Engine::registerListenSocketsForSelect | Read fd set and current max fd | Updated read fd set with all listening sockets | Registers accept-ready descriptors for the next select call. |
| 7 | Engine::registerClientSocketForSelect | Read fd set, write fd set, current max fd, active connections | Updated fd sets with client sockets and CGI pipe descriptors | Registers per-connection read/write readiness interests. |
| 8 | select | Max fd value and prepared read/write fd sets plus timeout | Number of ready descriptors and mutated readiness sets | Single readiness gate for socket and CGI pipe I/O. |
| 9 | Engine::checkTimeouts | Current time and active connection states | Timed-out connections converted to error responses or removed | Prevents indefinite hangs for headers, body reads, writes, and long CGI runs. |
| 10 | Engine::acceptPendingClientConnections | Ready listen fd set | New client connection objects stored in connection map | Accepts incoming clients and marks client socket non-blocking. |
| 11 | Engine::processIncomingData | Ready read fd set and active connections | Connection read buffers advanced or CGI output buffers advanced | Consumes inbound client bytes and CGI stdout bytes only when readable. |
| 12 | Engine::handleClientSocketRead | One client connection iterator and read fd set | Connection state transition to request-processing path or close path | Reads client socket bytes and forwards to request pipeline. |
| 13 | Engine::handleClientRequest | Connection object and freshly received bytes | Connection state updated to reading or writing | Coordinates header growth, request extraction, parsing, routing/CGI execution. |
| 14 | Engine::attemptIncomingHeader | Connection and new raw socket bytes | Updated read buffer and request state markers | Appends bytes and detects header terminator and size-limit violations. |
| 15 | Engine::processBufferedRequests | Connection and produced-response flag | Zero or more complete request units processed | Loops through complete requests already present in buffer. |
| 16 | Engine::handleRequestExtraction | Connection read buffer | One complete raw HTTP request string when available | Extracts full request frames including chunked-body completion checks. |
| 17 | validateAndExtractRequestFromBuffer | Mutable connection read buffer | Extracted raw request string and truncated buffer remainder | Performs framing validation and finds exact request boundary. |
| 18 | Engine::handleRequestParsing | Raw request string and effective body-size limit | Structured HttpRequest object or parse error response | Converts raw text to method, path, query, headers, cookies, and body. |
| 19 | HttpRequest::parse | Full raw HTTP request text and max-body-size value | Normalized request fields in HttpRequest object | Runs request-line parsing, header parsing, cookie parsing, and body extraction. |
| 20 | Engine::findBestLocation | Server config and normalized URL path | Best-matching location config pointer or null | Chooses route config for method rules, root mapping, CGI flags, and upload rules. |
| 21 | HttpRequest::shouldCloseConnectionByHttpRules | Parsed HTTP version and Connection header value | Boolean keep-alive decision | Computes per-request connection closing behavior. |
| 22 | Engine::handleRequestExecution | Connection, parsed request, selected server/location, keep-alive decision | Response buffer appended or CGI launched | Applies method authorization, session-cookie policy, then dispatches CGI or static routing. |
| 23 | Engine::ensureSessionCookieHeader | Parsed request cookies and in-memory session map | Optional Set-Cookie header string | Reuses valid session IDs or creates a new session token with TTL. |
| 24 | Branch point: CGI check inside handleRequestExecution | Request path, location CGI flags, extension rules | Either CGI launch path or normal route path | Decides dynamic execution versus static file/method route handling. |
| 25A | Engine::launchCGI | Connection, request, selected server/location, keep-alive decision | Connection moved to CGI running state or immediate error response | Starts CGI child process and non-blocking pipes for stdin/stdout exchange. |
| 26A | Engine::resolveCGIScriptPath | Request path and selected location root mapping | Filesystem script path string | Translates URL to script file path while enforcing traversal checks. |
| 27A | Engine::validateCGIScript | Script path and server config | Valid execution permission decision or error response | Confirms script exists, is regular file, and is executable. |
| 28A | Engine::createCGIProcess | Connection and server config | Forked process with stdin/stdout pipes or error response | Allocates pipes and forks child process for CGI execution. |
| 29A | Engine::setupCGIChildProcess | Pipe fds, script path, request, server config | execve call environment and process image replacement | Builds CGI environment variables and executes target script. |
| 30A | Engine::setupCGIParent | Connection, request body, pipe fds, child pid | Connection state set to CGI running with pipe context | Stores CGI runtime context for event-loop driven stdin/stdout handling. |
| 31A | Engine::processCGIOutput | CGI-running connection and ready read fd set | HTTP-formatted response written to connection write buffer | Reads CGI stdout to EOF and converts CGI headers/body into HTTP response. |
| 32A | Engine::buildResponseFromCGIOutput | Raw CGI output text and pending cookie header | Full HTTP response bytes in write buffer | Normalizes CGI output and injects Set-Cookie when needed. |
| 25B | Engine::routeRequest | Parsed request, selected server/location, keep-alive decision | Complete HTTP response string | Dispatches GET, POST, DELETE, redirects, and method-not-allowed responses. |
| 26B | Engine::handleGet | Request path and location root/index behavior | Static file response, autoindex response, or error response | Resolves file path, directory index fallback, and content type. |
| 27B | Engine::handlePost | Request body and upload location policy | Upload success response or policy/error response | Validates upload authorization and writes uploaded data to target directory. |
| 28B | Engine::handleDelete | Request path and resolved root path | Deletion success response or error response | Removes target resource when allowed and present. |
| 29B | Engine::buildStandardResponse and Engine::buildErrorResponse | Status code, body text, content type, connection-close choice, extra headers | RFC-style HTTP response bytes | Constructs final wire-format response with headers and body. |
| 33 | Engine::processOutgoingData | Ready write fd set and active connections | Client bytes sent and connection state transitioned or closed | Sends queued responses and writes CGI stdin when applicable. |
| 34 | Engine::handleCGIStdinWrite | CGI connection and ready write fd set | CGI stdin offset advanced or stdin closed | Feeds request body to CGI child stdin using non-blocking writes. |
| 35 | Engine::handleClientWrite | One client connection and ready write fd set | Response bytes drained to socket; state returns to READING or closes | Sends response bytes and finalizes keep-alive or connection close. |

## End-to-end output
The final output of this full chain is a standards-formatted HTTP response written to the client socket, followed by either keep-alive reuse of the same connection for the next request or a clean connection shutdown.
