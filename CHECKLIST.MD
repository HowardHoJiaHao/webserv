# Webserv Core Progress Report (Clean & Updated)

---

# ✅ Phase 1 — Configuration Data Model (Completed)

## 1. LocationConfig

* Implemented in C++98
* All members private
* Constructor uses initializer list
* `_uploadEnabled` initialized to `false`
* All getters `const`
* No runtime logic
* No raw pointers
* No uninitialized primitive members

---

## 2. ServerConfig

* Implemented in C++98
* `_port` initialized to `0`
* `_locations` stored as `std::vector<LocationConfig>`
* `addLocation()` implemented
* All getters `const`
* No parsing logic
* Clean separation from socket layer

---

## 3. Config

* Holds `std::vector<ServerConfig>`
* Dummy initialization via constructor
* No global state
* Immutable after construction

---

# ✅ Phase 2 — Runtime Socket Binding (Completed)

## 1. Engine Runtime Layer

* `Engine` orchestrates runtime
* Holds `const Config&`
* Strict separation between config and OS layer

---

## 2. Listening Socket Deduplication

* `std::map<std::pair<std::string, int>, int>`
* Exactly one listening socket per `(host, port)`
* Prevents duplicate `bind()` calls

**Architectural invariant:**
Each unique `(host, port)` produces exactly one listening socket.

---

## 3. Socket Creation

* Uses `getaddrinfo()`
* Applies `SO_REUSEADDR`
* Correct `bind()` and `listen()` sequence
* Proper error handling
* `addrinfo` freed correctly

---

## 4. Ownership Model

* Engine owns all listening FDs
* No FD stored inside configuration objects
* Clean OS boundary

---

# ✅ Phase 3 — Reactor & Connection Lifecycle (Completed)

## 1. Multiplexing Layer

* `select()` based reactor
* Dynamic `fd_set` per loop iteration
* Correct `maxFd` tracking
* Proper error handling
* No EBADF issues

---

## 2. Connection Abstraction

* `Connection` encapsulates:

  * Client FD
  * Read buffer
  * Write buffer
  * Closed flag
  * Protocol state
* Destructor-driven socket close
* Copy constructor & assignment disabled

---

## 3. Ownership Model

* `_connections` as `std::map<int, Connection*>`
* Safe deletion on disconnect or error
* Engine destructor cleans remaining connections
* No double-close
* No FD leak

---

## 4. Read Handling

* Accept new client connections
* Append incoming data to read buffer
* Iterator-safe erase during traversal

---

# ✅ Phase 4 — Non-Blocking Transport & Write State Machine (Completed)

## 1. Write Buffer Integration

* Response stored in `_writeBuffer`
* Write readiness monitored via `writeSet`
* Partial `send()` handled correctly
* Buffer erased progressively
* Connection deleted only after buffer empty

---

## 2. Non-Blocking Conversion

* Listening sockets set to `O_NONBLOCK`
* Accepted sockets set to `O_NONBLOCK`
* `recv()` handles `EAGAIN` / `EWOULDBLOCK`
* `send()` handles `EAGAIN` / `EWOULDBLOCK`

---

## 3. Validation & Stress Testing

* Verified with `curl`
* Verified with `nc`
* 2MB payload test successful
* Concurrent request stress test successful
* No hang
* No crash
* No truncation

Transport layer is fully stable and considered complete.

---

# ✅ Phase 5 — HTTP Framing & Minimal Parsing (In Progress)

## 5.1 Explicit Connection State (Completed)

* `enum State { READING, WRITING }` added to `Connection`
* `_state` member added
* Initialized to `READING` in constructor
* Explicit transition to `WRITING` after response generation
* Reactor `fd_set` population gated by connection state
* Read and write operations strictly separated by protocol state

**Architectural invariant:**
A connection in `READING` never sends.
A connection in `WRITING` never receives.

---

## 5.2 Header Boundary Detection (Next)

* [ ] Detect `"\r\n\r\n"` in read buffer
* [ ] Do not respond before header completion

---

## 5.3 Request Line Parsing

* [ ] Parse method, path, version
* [ ] Validate structure
* [ ] Return 400 on malformed request

---

## 5.4 Basic Header Parsing

* [ ] Parse header lines
* [ ] Store in key-value map
* [ ] Extract `Content-Length`

---

## 5.5 Content-Length Handling

* [ ] Wait for full body when present
* [ ] Only transition to WRITING when body complete

---

# 🎯 Phase 5 Completion Plateau

Stop when:

* Server responds only to complete, valid HTTP requests
* Invalid requests return 400
* POST with body works correctly
* No premature response
* Transport remains stable

Refactor before moving to next phase.

---

# 🎯 Current System State

The system now provides:

* Immutable configuration layer
* Deterministic socket binding layer
* Stable multiplexed reactor
* Fully non-blocking transport
* Explicit protocol state control
* Safe connection lifecycle management

Architecture layers completed:

1. Config Layer
2. Socket Binding Layer
3. Reactor Layer
4. Non-Blocking Transport Layer
5. Protocol State Layer

Transport correctness is complete.
Protocol framing is now under construction.

---

# Long-Term Roadmap (After Phase 5)

* HTTP request abstraction
* HTTP response abstraction
* Virtual host routing
* Static file serving
* Keep-alive support
* Timeout manager
* CGI handling

---

# Summary

Phase 1 — Stable
Phase 2 — Stable
Phase 3 — Stable
Phase 4 — Stable
Phase 5.1 — Complete
Phase 5.2+ — In Progress

The transport foundation is stable.
Protocol correctness is now being layered on top.
