# Webserv Core Progress Report (Re-Structured with Explicit Phase 4)

---

## ✅ Phase 1 — Configuration Data Model (Completed)

### 1. LocationConfig

* Implemented in C++98
* All members private
* Constructor uses initializer list
* `_uploadEnabled` initialized to `false`
* All getters `const`
* No runtime logic
* No raw pointers
* No uninitialized primitives

---

### 2. ServerConfig

* Implemented in C++98
* `_port` initialized to `0`
* `_locations` stored as `std::vector<LocationConfig>`
* `addLocation()` implemented
* All getters `const`
* No parsing logic
* Clean separation from socket layer

---

### 3. Config Class

* Holds `std::vector<ServerConfig>`
* Dummy initialization via constructor
* No global state
* Immutable after construction

---

## ✅ Phase 2 — Runtime Socket Binding (Completed)

### 1. Engine Runtime Layer

* `Engine` owns runtime orchestration
* Holds `const Config&`
* Clean separation from configuration layer

---

### 2. Listening Socket Deduplication

* `std::map<std::pair<std::string, int>, int>`
* One socket per unique `(host, port)`
* Prevents duplicate `bind()`

Architectural invariant:

> Each unique (host, port) produces exactly one listening socket.

---

### 3. Socket Creation Logic

* `getaddrinfo()` used
* `SO_REUSEADDR` applied
* `bind()` and `listen()` implemented
* Proper failure handling
* `addrinfo` freed correctly

---

### 4. Ownership Model

* Engine owns listening FDs
* No FD inside config layer
* Clear OS boundary separation

---

## ✅ Phase 3 — Reactor & Connection Lifecycle (Completed)

### 1. Multiplexing Layer

* `select()` based reactor
* Dynamic `fd_set` per iteration
* Correct `maxFd` tracking
* Proper error handling

---

### 2. Connection Abstraction

* `Connection` encapsulates:

  * Client FD
  * Read buffer
  * Write buffer
  * Closed state
  * Protocol state (`READING` / `WRITING`)
* Destructor-driven socket close
* Non-copyable resource owner

---

### 3. Ownership Model

* `_connections` as `std::map<int, Connection*>`
* Proper deletion on disconnect
* Engine destructor cleans remaining connections
* No double-close
* No FD leak

---

### 4. Read Handling

* Accept new client connections
* Append incoming data to read buffer
* Iterator-safe erase pattern

---

## ✅ Phase 4 — Non-Blocking Transport & Write Handling (Completed)

### 1. Write Buffer Integration

* Response stored in `_writeBuffer`
* Write FD monitored via `writeSet`
* Partial `send()` handled
* Buffer erased progressively
* Connection deleted only after buffer empty

---

### 2. Non-Blocking Conversion

* Listening sockets set to `O_NONBLOCK`
* Accepted sockets set to `O_NONBLOCK`
* `recv()` handles `EAGAIN` / `EWOULDBLOCK`
* `send()` handles `EAGAIN` / `EWOULDBLOCK`

---

### 3. Stress Testing

* Large payload (2MB) verified
* Concurrent curl requests verified
* No hang, no crash, no truncation

Transport layer is considered stable and ready for protocol layering.

---

## ✅ Phase 5 — HTTP Framing & Minimal Parsing (In Progress)

### 5.1 Explicit Connection State (Completed)

* `enum State { READING, WRITING }` added
* `_state` member added to `Connection`
* Initialized to `READING` in constructor
* Explicit transition to `WRITING` after response preparation
* Reactor `fd_set` population gated by connection state
* Read/write execution strictly separated by protocol state

Architectural invariant:

> A connection in `READING` never sends.
> A connection in `WRITING` never receives.

Protocol state now explicitly controls transport behavior.

---

### 5.2 HTTP Header Boundary Detection (Next)

* [ ] Detect `"\r\n\r\n"` in read buffer
* [ ] Only schedule write after header completion
* [ ] Ignore incomplete requests

---

### 5.3 Minimal Request Line Parsing

* [ ] Parse method, path, version
* [ ] Validate request line structure
* [ ] Return 400 on malformed request

---

### 5.4 Basic Header Parsing

* [ ] Parse headers into key-value map
* [ ] Extract `Content-Length` if present

---

### 5.5 Content-Length Handling

* [ ] Wait for full body if present
* [ ] Only transition to `WRITING` when complete

---

# 🎯 Stopping Point for Phase 5

* Server responds only to complete, valid HTTP requests
* Invalid requests return 400
* POST with body handled correctly
* No premature response before full message
* Transport stability preserved

---

# 🎯 Current System State

Completed layers:

1. Config Layer
2. Socket Binding Layer
3. Reactor Layer
4. Non-Blocking Transport Layer
5. Protocol State Layer

System now provides a correct, fully non-blocking, multiplexed TCP server core with explicit protocol state control.

---

# Long-Term Roadmap (After Phase 5)

* HTTP request abstraction class
* HTTP response abstraction class
* Virtual host routing
* Static file serving
* Keep-alive support
* Timeout manager
* CGI handling

These are later phases, not immediate tasks.

---

# Summary

Phase 1 — Stable
Phase 2 — Stable
Phase 3 — Stable
Phase 4 — Stable
Phase 5.1 — Complete
Phase 5.2+ — In Progress

Transport foundation is stable.
Protocol framing is now being layered on top in a controlled manner.
