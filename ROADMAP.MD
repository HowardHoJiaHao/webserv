# Webserv — Pending & Upcoming Phases (Master Roadmap)

---

# 🧱 Completed Foundation

✔ Phase 1 — Configuration Model
✔ Phase 2 — Socket Binding
✔ Phase 3 — Reactor & Connection Lifecycle
✔ Phase 4 — Non-Blocking Transport & Write Handling

Transport layer is complete and stable.

We now build upward.

---

# 🚀 Phase 5 — HTTP Framing & Minimal Protocol Awareness

## 🎯 Objective

Respond only to complete and valid HTTP requests.

---

## 5.1 — Protocol State Introduction

* [ ] Add `enum State { READING, WRITING }`
* [ ] Add `_state` to `Connection`
* [ ] Initialize to `READING`
* [ ] Transition only when request complete

---

## 5.2 — Header Boundary Detection

* [ ] Detect `"\r\n\r\n"`
* [ ] Do not respond before headers complete
* [ ] Ignore incomplete requests

---

## 5.3 — Request Line Parsing

* [ ] Parse method, path, version
* [ ] Validate exactly 3 tokens
* [ ] Return 400 for malformed requests

---

## 5.4 — Header Parsing

* [ ] Parse headers into key-value map
* [ ] Extract `Content-Length`
* [ ] Validate numeric values

---

## 5.5 — Body Handling

* [ ] Wait for full body based on Content-Length
* [ ] Only transition to WRITING when body complete
* [ ] Reject incomplete or invalid bodies

---

## 🛑 Phase 5 Stop Condition

✔ Only valid HTTP triggers response
✔ Invalid requests return 400
✔ POST with body works correctly
✔ No premature response
✔ Transport layer remains stable

Refactor and clean before moving forward.

---

# 🚀 Phase 6 — HTTP Structure Abstraction

## 🎯 Objective

Separate protocol parsing from connection handling.

---

## 6.1 — HttpRequest Class

* [ ] method
* [ ] path
* [ ] version
* [ ] headers map
* [ ] body
* [ ] validation logic

---

## 6.2 — HttpResponse Class

* [ ] status code
* [ ] headers map
* [ ] body
* [ ] serialize() method

---

## 🛑 Phase 6 Stop Condition

✔ Connection no longer manually parses strings
✔ Engine no longer manually builds raw response strings
✔ Protocol logic isolated from transport layer

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

* [ ] Explicit transitions
* [ ] Remove implicit buffer-based logic
* [ ] Deterministic lifecycle model

---

## 🛑 Phase 7 Stop Condition

✔ Clear state diagram exists
✔ No hidden transitions
✔ No ambiguous lifecycle behavior

---

# 🚀 Phase 8 — Static File Serving

## 🎯 Objective

Serve real files from filesystem.

---

* [ ] Map path to filesystem
* [ ] Open file safely
* [ ] Determine file size
* [ ] Set correct Content-Length
* [ ] Handle 404
* [ ] Handle permission errors

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

* [ ] Parse Host header
* [ ] Select correct ServerConfig
* [ ] Match location blocks
* [ ] Apply root/index rules

---

## 🛑 Phase 9 Stop Condition

✔ Multiple server blocks behave differently
✔ Routing logic is config-driven

---

# 🚀 Phase 10 — Advanced Features (Later)

Do NOT implement until all above layers are stable.

* [ ] Keep-Alive support
* [ ] Timeout manager
* [ ] Chunked transfer encoding
* [ ] Upload handling
* [ ] CGI execution

---

# 🧠 Architectural Rule

Build vertically:

Transport → Framing → Parsing → Abstraction → Routing → Resources → Features

Never jump layers.
Never implement advanced features on unstable foundation.

---

# 🎯 Immediate Next Action

Start Phase 5.1 only.
Stop after explicit state works cleanly.
Review before moving forward.
