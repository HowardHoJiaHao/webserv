Got it. This time: **full updated file, clean, no implicit stuff, copy-paste ready.**

---

````markdown
# Webserv — Pending & Upcoming Phases (Master Roadmap — UPDATED)

---

# 🧱 Completed Foundation

✔ Phase 1 — Configuration Model  
✔ Phase 2 — Socket Binding  
✔ Phase 3 — Reactor & Connection Lifecycle  
✔ Phase 4 — Non-Blocking Transport & Write Handling  
✔ Phase 5 — HTTP Framing & Minimal Protocol Enforcement  
✔ Phase 6 — HTTP Structure Abstraction  
✔ Phase 7 — Routing Layer  

---

# 🧠 System Reality

```text
Transport Layer        → COMPLETE
Framing Layer          → COMPLETE
Protocol Parsing       → COMPLETE
Protocol Semantics     → COMPLETE
Routing Layer          → COMPLETE
Architecture Separation→ COMPLETE
````

---

# 🚀 Phase 8 — Static File Serving (CURRENT)

## 🎯 Objective

Serve real files instead of hardcoded strings.

---

## 8.1 — Path Mapping

```text
"/"      → ./www/index.html
"/abc"   → ./www/abc
```

---

## 8.2 — Required Implementation

✔ open() file
✔ read() file content
✔ build HTTP response
✔ set Content-Length
✔ send file content

---

## 8.3 — Minimal Implementation Flow

```text
request.getPath()
→ map to file path
→ open()
→ read()
→ build response
→ send
```

---

## 8.4 — Error Handling

✔ file not found → 404
✔ permission denied → 403
✔ invalid path → reject

---

## 8.5 — Minimal MIME Support

```text
.html → text/html
.txt  → text/plain
```

(keep it minimal first)

---

## 🛑 Phase 8 Stop Condition

✔ `/` returns actual file
✔ `/abc` returns file if exists
✔ missing file returns 404
✔ curl shows correct content

---

# 🚀 Phase 9 — POST Handling

## 🎯 Objective

Handle request body properly.

---

## Required

✔ use request.getBody()
✔ support Content-Length
✔ process POST data

---

# 🚀 Phase 10 — CGI Execution

## 🎯 Objective

Execute scripts.

---

## Required

✔ fork()
✔ execve()
✔ pipe()
✔ send output as response

---

# 🚀 Phase 11 — Config Integration

## 🎯 Objective

Use config to control behavior.

---

## Required

✔ Host header handling
✔ select ServerConfig
✔ apply root
✔ apply index

---

# 🚀 Phase 12 — Robustness & Edge Cases

## Required

✔ path traversal protection (`../`)
✔ timeout handling
✔ large file support
✔ error handling consistency

---

# 🧠 Architecture Rule (DO NOT BREAK)

```text
Transport → Parsing → Routing → File → Features
```

---

# 🎯 Current Position

```text
YOU ARE HERE:
Phase 8 — Static File Serving
```

---

# 🎯 Immediate Next Action

```text
1. Create serveStaticFile()
2. Replace buildIndexResponse() with file read
3. Test with curl
```

---

# Summary

Phase 1 — Stable
Phase 2 — Stable
Phase 3 — Stable
Phase 4 — Stable
Phase 5 — Complete
Phase 6 — Complete
Phase 7 — Complete
Phase 8 — CURRENT

```

---

