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

---

## 5.3 Request Line Parsing (Completed)

* Parse method, path, version
* Validate exactly three tokens
* Validate path begins with `/`
* Validate HTTP version prefix

---

## 5.4 Basic Header Parsing (Completed)

* Extract header section
* Validate `Key: Value`
* Temporary header map created

---

## 5.5 Content-Length Enforcement (Completed)

* Body size tracked via buffer
* Wait until full body arrives
* Prevent premature response

---

# 🎯 Phase 5 Completion Plateau (Reached)

✔ Structural HTTP correctness achieved
✔ No premature responses
✔ Stable under partial reads

---

# ✅ Phase 6 — HTTP Abstraction Layer (NEWLY COMPLETED)

This phase introduces a critical architectural separation.

---

## 6.1 HttpRequest Introduction

* New abstraction layer created
* Encapsulates full request state:

  * Method
  * Path
  * Version
  * Headers
  * Body
  * Content-Length

---

## 6.2 Parsing Responsibility Migration

Moved from Engine → HttpRequest:

✔ Request line parsing
✔ Header parsing
✔ Content-Length validation
✔ Body extraction

Engine no longer parses protocol.

---

## 6.3 Structured Parsing Pipeline

Strict execution order enforced:

1. Request line
2. Header boundary
3. Header parsing
4. Semantic interpretation
5. Body extraction

---

## 6.4 Content-Length Semantics

* Stored internally
* Validated for numeric correctness
* Empty values rejected

---

## 6.5 Body Ownership

* `_body` stored inside HttpRequest
* Uses Content-Length when available
* Prevents over-reading

---

## 6.6 Completeness Logic

* Introduced:

  bool isComplete(size_t currentBodySize)

* Determines when request is ready

---

## 6.7 Engine Refactor

Engine now ONLY:

✔ receives data
✔ appends buffer
✔ calls parse()
✔ checks isComplete()
✔ sends response

Removed from Engine:

✘ header parsing
✘ Content-Length validation
✘ protocol decisions

---

## 6.8 Architectural Invariant (CRITICAL)

Transport and protocol are fully decoupled:

Engine → transport layer
HttpRequest → protocol layer

---

# 🎯 Phase 6 Completion Plateau (Reached)

✔ Full HTTP request abstraction complete
✔ No duplicated parsing logic
✔ No protocol leakage into Engine
✔ Clean layered architecture established

---

# 🚀 Phase 7 — Routing Layer (CURRENT)

## Objective

Transform parsed request into behavior.

---

## 7.1 Routing Decision Layer

* Use:

  * request.getMethod()
  * request.getPath()

* Map request → handler

---

## 7.2 Initial Routing Scope

* GET / → index response
* Unknown path → 404 response

---

## 7.3 Architectural Role

Routing bridges:

Protocol → Application logic

---

# 🔜 Upcoming Phases

## Phase 8 — Static File Serving

## Phase 9 — POST Handling

## Phase 10 — CGI Execution

## Phase 11 — Config Integration

## Phase 12 — Robustness & Edge Cases

---

# 📊 System Status

✔ Infrastructure complete
✔ Protocol complete
✔ Architecture stable

🔄 Application layer in progress

---

# Summary

Phase 1 — Stable
Phase 2 — Stable
Phase 3 — Stable
Phase 4 — Stable
Phase 5 — Complete
Phase 6 — Complete
Phase 7 — In Progress

System is now transitioning from correctness → behavior.
