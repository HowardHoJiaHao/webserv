# Webserv — Pending & Upcoming Phases (Master Roadmap — CORRECTED CONTINUATION)

---

# 🧱 Completed Foundation (REALIGNED)

✔ Phase 1 — Configuration Model
✔ Phase 2 — Socket Binding
✔ Phase 3 — Reactor & Connection Lifecycle
✔ Phase 4 — Non-Blocking Transport & Write Handling
✔ Phase 5 — HTTP Framing & Minimal Protocol Enforcement
✔ Phase 6 — HTTP Structure Abstraction (**COMPLETED — CRITICAL MILESTONE**)

---

## 🧠 System Reality (CRITICAL UPDATE)

```text
Transport Layer        → COMPLETE
Framing Layer          → COMPLETE
Protocol Parsing       → COMPLETE
Protocol Semantics     → COMPLETE
Architecture Separation→ COMPLETE
```

👉 The system is no longer “pre-abstraction”.
👉 It is now in **post-abstraction phase**.

---

# ✅ Phase 5 — HTTP Framing & Minimal Protocol Awareness (Completed)

(UNCHANGED — retained for continuity)

---

# ✅ Phase 6 — HTTP Structure Abstraction (**COMPLETED — UPDATED**)

## 🎯 Objective (ACHIEVED)

Separate protocol parsing from connection handling.

---

## 6.1 — HttpRequest Class (IMPLEMENTED)

### Data Model

* `_method`
* `_path`
* `_version`
* `_headers`
* `_body`
* `_contentLength`
* `_hasContentLength`

---

### Responsibilities (ACHIEVED)

✔ Own full parsing pipeline
✔ Own validation rules
✔ Own semantic interpretation
✔ Own body extraction
✔ Provide read-only access to request data

---

## 6.2 — Parsing Pipeline (STRICT ORDER — ENFORCED)

```text
1. Request line parsing
2. Header boundary detection
3. Header parsing
4. Content-Length interpretation
5. Body extraction (length-aware)
```

---

## 6.3 — Semantic Enforcement (Content-Length)

✔ Empty rejected
✔ Non-numeric rejected
✔ Stored as integer
✔ Presence tracked via `_hasContentLength`

---

## 6.4 — Body Handling (CORRECTED)

✔ Extracted after semantics known
✔ Uses exact Content-Length
✔ Prevents over-read
✔ Compatible with partial buffer accumulation

---

## 6.5 — Completeness Logic (CRITICAL)

```cpp
bool isComplete(size_t currentBodySize)
```

Rules:

* No Content-Length → complete immediately
* With Content-Length → wait for full body

---

## 6.6 — Engine Refactor (MAJOR ARCHITECTURAL SHIFT)

### Engine NO LONGER:

✘ parses headers
✘ validates Content-Length
✘ interprets HTTP

---

### Engine NOW ONLY:

✔ handles socket I/O
✔ appends buffers
✔ invokes parsing
✔ checks completeness
✔ triggers response

---

## 6.7 — HttpResponse (PARTIALLY PENDING)

⚠️ IMPORTANT: This part is NOT fully implemented yet in your system.

You are still likely doing:

```cpp
buildMinimalResponse()
```

Instead of:

```text
HttpResponse → serialize()
```

---

### Required Work (STILL MISSING)

* status code abstraction
* header map for response
* content-length generation
* serialization function

---

## 🛑 Phase 6 Stop Condition (REVISED — TRUE COMPLETION)

✔ HttpRequest fully owns protocol parsing
✔ Engine contains zero protocol logic
✔ Body extraction is correct
✔ isComplete() used for control flow

⚠️ HttpResponse abstraction may still be pending

---

# 🚀 Phase 7 — Routing Layer (**ACTUAL CURRENT PHASE**)

---

## 🎯 Objective

Convert request into behavior.

```text
HttpRequest → decision → response
```

---

## 7.1 — Routing Inputs

* request.getMethod()
* request.getPath()

---

## 7.2 — Minimal Routing Implementation

```cpp
if (method == "GET")
{
    if (path == "/")
        return index;
    else
        return 404;
}
```

---

## 7.3 — Responsibilities

Routing decides:

✔ WHAT to serve
✘ NOT how to serve

---

## 7.4 — Constraints

* No filesystem yet
* No config usage yet
* No CGI

👉 Keep routing purely logical

---

## 🛑 Phase 7 Stop Condition

✔ Different paths produce different responses
✔ 404 implemented
✔ Method branching exists
✔ No single hardcoded response

---

# 🚀 Phase 8 — Static File Serving

---

## 🎯 Objective

Serve actual filesystem resources.

---

## 8.1 — Path Mapping

```text
/request/path → ./www/request/path
```

---

## 8.2 — Implementation Details

✔ open() file
✔ read() file content
✔ compute size
✔ set Content-Length
✔ send bytes correctly

---

## 8.3 — Error Handling

✔ file not found → 404
✔ permission denied → 403
✔ invalid path → reject

---

## 8.4 — MIME Types (basic)

```text
.html → text/html
.css → text/css
.jpg → image/jpeg
.png → image/png
```

---

## 🛑 Phase 8 Stop Condition

✔ Browser renders HTML
✔ CSS/images load
✔ Binary files work

---

# 🚀 Phase 9 — Virtual Host & Config Routing

---

## 🎯 Objective

Integrate config into routing.

---

## 9.1 — Required Features

✔ parse Host header
✔ select correct ServerConfig
✔ match location blocks
✔ apply root directive
✔ apply index rules

---

## 🛑 Phase 9 Stop Condition

✔ Multiple virtual hosts work
✔ Behavior changes via config
✔ No hardcoded routing

---

# 🚀 Phase 10 — Advanced Features (STRICTLY AFTER STABILITY)

---

## 🎯 Objective

Extend server capabilities.

---

## 10.1 — Features

* Keep-Alive
* Timeout manager
* Chunked encoding
* Upload handling
* CGI execution

---

## ⚠️ Critical Warning

Do NOT start this phase early.

---

# 🧠 Architectural Rule (UNCHANGED — ENFORCED)

```text
Transport → Framing → Parsing → Abstraction → Routing → Resources → Features
```

Violating order will cause regression.

---

# 🎯 Immediate Next Action (CORRECTED)

You are NOT starting Phase 6.

You are HERE:

```text
Phase 7 — Routing
```

---

## DO THIS NEXT

```text
1. Remove buildMinimalResponse()
2. Implement path-based branching
3. Return index for "/"
4. Return 404 otherwise
```

---

# 🧠 Final System Position

```text
✔ Core architecture complete
✔ Protocol correctness complete
✔ Transport stability complete

→ Moving into behavior layer
```

---

# Summary

Phase 1 — Stable
Phase 2 — Stable
Phase 3 — Stable
Phase 4 — Stable
Phase 5 — Complete
Phase 6 — Complete
Phase 7 — CURRENT

System has transitioned from correctness → functionality.
