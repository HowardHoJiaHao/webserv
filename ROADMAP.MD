# Webserv — Pending & Upcoming Phases (Master Roadmap)

---

# 🧱 Completed Foundation

✔ Phase 1 — Configuration Model
✔ Phase 2 — Socket Binding
✔ Phase 3 — Reactor & Connection Lifecycle
✔ Phase 4 — Non-Blocking Transport & Write Handling
✔ Phase 5 — HTTP Framing & Minimal Protocol Enforcement

Transport layer is complete and stable.
HTTP framing, structural validation, and body enforcement are complete.

We now move upward into architectural separation.

---

# ✅ Phase 5 — HTTP Framing & Minimal Protocol Awareness (Completed)

## 🎯 Objective (Achieved)

Respond only to complete and valid HTTP requests.

---

## 5.1 — Protocol State Introduction (Completed)

* `enum State { READING, WRITING }` implemented
* `_state` added to `Connection`
* Initialized to `READING`
* Explicit transition to `WRITING` after response preparation
* Reactor `fd_set` population gated by state
* Read/write execution strictly separated by protocol state

Architectural invariant:

> A connection in `READING` never sends.
> A connection in `WRITING` never receives.

---

## 5.2 — Header Boundary Detection (Completed)

* Detect `"\r\n\r\n"` in read buffer
* No response before header completion
* Incomplete headers buffered safely
* Framing strictly enforced

---

## 5.3 — Request Line Parsing (Completed)

* Parse method, path, version
* Validate exactly 3 tokens
* Validate path begins with `/`
* Validate HTTP version prefix
* Return 400 for malformed requests

---

## 5.4 — Header Parsing (Completed)

* Parse headers into key-value map
* Validate `Key: Value` format
* Extract `Content-Length`
* Reject empty or non-numeric values
* Return 400 on malformed headers

---

## 5.5 — Body Handling (Completed)

* Compute body start offset
* Calculate current body size
* If `Content-Length` present:

  * Wait until full body received
  * Do not respond prematurely
* Only transition to `WRITING` when body complete

---

## 🛑 Phase 5 Stop Condition (Reached)

✔ Only valid HTTP triggers response
✔ Invalid requests return 400
✔ POST with body works correctly
✔ No premature response
✔ Transport layer remains stable

Phase 5 is structurally complete.

---

# 🚀 Phase 6 — HTTP Structure Abstraction (Next)

## 🎯 Objective

Separate protocol parsing from connection handling.

Current issue:
Parsing logic still lives inside `Engine::handleClientRequest()`.

Goal: Move protocol awareness out of transport layer.

---

## 6.1 — HttpRequest Class

* method
* path
* version
* headers map
* body
* validation logic
* parsing constructor or factory method

Responsibilities:

* Own parsing logic
* Own validation rules
* Represent immutable request data after parsing

---

## 6.2 — HttpResponse Class

* status code
* headers map
* body
* `serialize()` method

Responsibilities:

* Build structured response
* Serialize into correct HTTP wire format
* Remove raw string building from `Engine`

---

## 🛑 Phase 6 Stop Condition

✔ Connection no longer manually parses strings
✔ Engine no longer manually builds raw response strings
✔ Protocol logic isolated from transport layer
✔ Clear separation: Transport vs Protocol

---

# 🚀 Phase 7 — Formal Connection State Machine

## 🎯 Objective

Make lifecycle deterministic and explicit.

---

States expand to:

* READING_HEADERS
* READING_BODY
* WRITING
* CLOSING

---

* Explicit transitions only
* Remove implicit buffer-length logic
* Deterministic lifecycle model
* Clear state diagram documented

---

## 🛑 Phase 7 Stop Condition

✔ Clear state diagram exists
✔ No hidden transitions
✔ No ambiguous lifecycle behavior
✔ No buffer-length driven implicit logic

---

# 🚀 Phase 8 — Static File Serving

## 🎯 Objective

Serve real files from filesystem.

---

* Map path to filesystem
* Open file safely
* Determine file size
* Set correct Content-Length
* Handle 404
* Handle permission errors
* Support binary file serving

---

## 🛑 Phase 8 Stop Condition

✔ Browser loads static HTML file correctly
✔ 404 works
✔ Binary files work

---

# 🚀 Phase 9 — Virtual Host Routing

## 🎯 Objective

Use Host header and config to select server.

---

* Parse Host header
* Select correct ServerConfig
* Match location blocks
* Apply root/index rules
* Config-driven routing logic

---

## 🛑 Phase 9 Stop Condition

✔ Multiple server blocks behave differently
✔ Routing logic fully config-driven

---

# 🚀 Phase 10 — Advanced Features (Later Only)

Do NOT implement until all above layers are stable.

* Keep-Alive support
* Timeout manager
* Chunked transfer encoding
* Upload handling
* CGI execution

---

# 🧠 Architectural Rule

Build vertically:

Transport → Framing → Parsing → Abstraction → Routing → Resources → Features

Never jump layers.
Never implement advanced features on unstable foundation.

---

# 🎯 Immediate Next Action

Start Phase 6.

Extract HTTP parsing logic into `HttpRequest`.
Extract response construction into `HttpResponse`.
Refactor `Engine` into pure transport + state orchestration.

Protocol correctness is stable.
Now enforce architectural separation.
