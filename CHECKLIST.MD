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

# ✅ Phase 6 — HTTP Abstraction Layer (Completed)

## Key Achievements

✔ HttpRequest introduced as protocol owner
✔ Parsing fully moved out of Engine
✔ Content-Length handled internally
✔ Body extraction handled internally
✔ isComplete() determines readiness

---

## Architectural Invariant

Engine → transport + routing + execution
HttpRequest → protocol only

---

# 🎯 Phase 6 Completion Plateau (Reached)

✔ No protocol leakage into Engine (parsing isolated)
✔ Clean separation achieved for protocol layer

---

# ✅ Phase 7 — Routing Layer (Completed)

## 7.1 Routing Logic

* routeRequest(const HttpRequest& request)
* No global state
* Central decision layer

---

## 7.2 Supported Behavior

* GET / → 200 OK (index)
* GET unknown path → 404 Not Found
* Non-GET method → 405 Method Not Allowed
* Invalid request → 400 Bad Request

---

## 7.3 Execution Flow

recv → append buffer
→ parse()
→ isComplete()
→ routeRequest()
→ send()

---

# 🎯 Phase 7 Completion Plateau (Reached)

✔ Correct HTTP status codes
✔ Clean routing layer
✔ No protocol leakage

---

# 🚀 Phase 8 — Static File Serving (COMPLETED)

## 8.1 — Path Mapping

* `/` → `./www/index.html`
* `/abc` → `./www/abc`

---

## 8.2 — File Existence Check

* Implemented using `stat()`
* Missing file → 404

---

## 8.3 — File Reading

* `open → read (loop) → close`
* Handles large files correctly

---

## 8.4 — Response Construction

* Centralized `buildResponse()`
* Includes Content-Length & Content-Type

---

## 8.5 — MIME Support

* `.html` → text/html
* `.txt` → text/plain
* default → application/octet-stream

---

## 8.6 — Directory Handling

* Detect directory via `stat`
* Append `/index.html`

---

## 8.7 — Path Traversal Protection

* Reject paths containing `..`
* Return 403 Forbidden

---

## 8.8 — Concurrency Validation

* Parallel curl tests passed
* No crash, no hang

---

# 🎯 Phase 8 Completion

✔ Real file serving
✔ Correct headers
✔ MIME support
✔ Directory handling
✔ Path safety
✔ Stable under load

---

# 🚀 Phase 9 — POST Handling (COMPLETED)

## 9.1 — Request Body Integration

* HttpRequest guarantees full body accumulation
* Content-Length enforced strictly
* isComplete() includes body completeness

**Invariant:**
No request processed before full body is received

---

## 9.2 — Incremental Body Handling

* Supports partial reads across multiple recv()
* Body updated progressively
* Body only valid when complete

**Invariant:**
isComplete() == true ⇔ body is valid

---

## 9.3 — POST Routing Integration

* routeRequest extended with POST dispatch
* Clean separation from GET logic

---

## 9.4 — POST Execution

* Body written to filesystem
* Target file: `./www/upload.txt`

Uses:

* open (O_CREAT | O_WRONLY | O_TRUNC)
* loop-based write()

---

## 9.5 — Execution Safety

* Execution only happens when request is complete
* Prevents partial write and corruption

---

## 9.6 — Error Handling

* open failure → 500
* write failure → 500

---

## 9.7 — End-to-End Flow

recv → buffer
→ parse (headers + body)
→ isComplete
→ routeRequest
→ POST execution
→ send

---

# 🎯 Phase 9 Completion

✔ POST supported
✔ Safe body handling
✔ Correct file write
✔ No partial execution
✔ Stable under load

---

# 📊 System Status (UPDATED)

✔ Transport Layer        → COMPLETE
✔ Framing Layer          → COMPLETE
✔ Protocol Parsing       → COMPLETE
✔ Routing Layer          → COMPLETE
✔ Static File Serving    → COMPLETE
✔ POST Handling          → COMPLETE

---

# 🔜 Upcoming Phases

## Phase 10 — CGI Execution

* fork / execve / pipe

---

## Phase 11 — Config Integration

* Host-based routing
* root/index from config

---

## Phase 12 — Robustness & Edge Cases

* timeout
* error handling consistency

---

# Summary

Phase 1 — Stable
Phase 2 — Stable
Phase 3 — Stable
Phase 4 — Stable
Phase 5 — Complete
Phase 6 — Complete
Phase 7 — Complete
Phase 8 — Complete
Phase 9 — Complete
