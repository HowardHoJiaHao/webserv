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

# ✅ Phase 5 — HTTP Framing & Minimal Parsing (Completed)

Protocol layer now enforces structural correctness and body completeness.

---

## 5.1 Explicit Connection State (Completed)

* `enum State { READING, WRITING }` implemented
* `_state` added to `Connection`
* Initialized to `READING`
* Reactor `fd_set` population gated by state
* Read/write execution strictly separated by protocol state

**Architectural invariant:**
A connection in `READING` never sends.
A connection in `WRITING` never receives.

---

## 5.2 Header Boundary Detection (Completed)

* Detect `"\r\n\r\n"` in read buffer
* No response before header completion
* Partial headers safely buffered
* Framing strictly enforced

---

## 5.3 Request Line Parsing (Completed)

* Parse method, path, version
* Validate exactly three tokens
* Validate path begins with `/`
* Validate HTTP version prefix
* Return 400 on malformed request

---

## 5.4 Basic Header Parsing (Completed)

* Extract header section safely
* Split into individual header lines
* Skip request line during header parsing
* Validate `Key: Value` format
* Store headers in temporary key-value map
* Validate `Content-Length` header:

  * Reject empty value
  * Reject non-numeric value
* Return 400 on malformed headers

---

## 5.5 Content-Length Enforcement (Completed)

* Compute body start offset after header boundary
* Calculate current body size using buffer length
* If `Content-Length` exists:

  * Wait until full body is received
  * Do not transition to `WRITING` prematurely
* Only respond when:

  * Header is valid
  * Body is complete (if required)

**Protocol invariant:**
Server never responds before full declared body is received.

---

## 🎯 Phase 5 Completion Plateau (Reached)

✔ Server responds only to complete, valid HTTP requests
✔ Invalid requests return 400
✔ POST with body works correctly
✔ No premature response
✔ Transport layer remains stable

Phase 5 is structurally complete.

---

# 🎯 Current System State

The system now provides:

* Immutable configuration layer
* Deterministic socket binding layer
* Stable multiplexed reactor
* Fully non-blocking transport
* Explicit protocol state control
* Strict HTTP framing enforcement
* Safe connection lifecycle management

Architecture layers completed:

1. Config Layer
2. Socket Binding Layer
3. Reactor Layer
4. Non-Blocking Transport Layer
5. HTTP Framing & Structural Parsing Layer

Transport correctness is complete.
Protocol framing and minimal HTTP enforcement are complete.

---

# 🚀 Next Phase — HTTP Abstraction Layer (Phase 6)

Upcoming objectives:

* Introduce `HttpRequest` abstraction
* Introduce `HttpResponse` abstraction
* Remove string-based parsing from `Engine`
* Isolate protocol logic from transport logic

---

# Summary

Phase 1 — Stable
Phase 2 — Stable
Phase 3 — Stable
Phase 4 — Stable
Phase 5 — Complete

The transport foundation is stable.
HTTP framing and structural correctness are enforced.
The system is ready for protocol abstraction refactoring.
