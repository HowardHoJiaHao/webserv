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

Transport layer is now considered fully stable and production-grade for further protocol layering.

---

# 🎯 Current System State

Completed layers:

1. Config Layer
2. Socket Binding Layer
3. Reactor Layer
4. Non-Blocking Transport Layer

System now provides a correct, fully non-blocking, multiplexed TCP server core.

---

# 🔜 Phase 5 — HTTP Framing & Minimal Parsing (Next)

## Objective

Move from:

> Respond to any received bytes

To:

> Respond only to complete HTTP requests

---

## Phase 5 Checklist

### 1. Explicit Connection State

* [ ] Add `enum State { READING, WRITING }`
* [ ] Add `_state` to `Connection`
* [ ] Use explicit state transitions

---

### 2. HTTP Header Boundary Detection

* [ ] Detect `"\r\n\r\n"` in read buffer
* [ ] Only schedule write after header complete
* [ ] Ignore incomplete requests

---

### 3. Minimal Request Line Parsing

* [ ] Parse method, path, version
* [ ] Validate request line structure
* [ ] Return 400 on malformed request

---

### 4. Basic Header Parsing

* [ ] Parse headers into key-value map
* [ ] Extract `Content-Length` if present

---

### 5. Content-Length Handling

* [ ] Wait for full body if present
* [ ] Only transition to WRITING when complete

---

# Stopping Point for Phase 5

* Server only responds to valid complete HTTP requests
* Invalid requests return 400
* POST with body is handled correctly
* No premature response before full message
* Transport remains stable

---

# Long-Term Roadmap (After Phase 5)

* HTTP request/response abstraction classes
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
Phase 3 — Stable (Reactor & Lifecycle)
Phase 4 — Stable (Non-Blocking Transport)
Phase 5 — Next Target (HTTP Framing & Parsing)

System foundation is now strong enough to build protocol logic cleanly without revisiting transport design.
