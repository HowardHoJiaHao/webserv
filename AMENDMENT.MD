You don’t need something fancy. You need something **brutally clear and actionable**, so even a half-asleep AI (or your future self at 3AM) can’t misunderstand it.

Here. This is your **“don’t-fail-webserv” spec**:

---

# 🧾 WEB SERV — PASS CHECKLIST (AI-READY)

## 1. Request Parsing (CRITICAL)

**Goal:** Handle *real TCP behavior*, not ideal input.

### Must support:

* [x] Partial reads (request arrives in multiple `recv`)
* [x] Multiple requests in one buffer
* [x] Headers parsing stops exactly at `\r\n\r\n`
* [x] Body reading based on:

  * [x] `Content-Length`
  * [x] `Transfer-Encoding: chunked`

### Chunked requirements:

* [x] Parse hex chunk size
* [x] Read chunk data correctly
* [x] Stop at `0\r\n\r\n`

### Must reject:

* [x] Invalid headers
* [x] Conflicting headers (e.g. multiple Content-Length)

---

## 2. Request State Machine (MANDATORY DESIGN)

**Goal:** No “one-shot parsing” nonsense.

### Required states:

```
READING_HEADERS
→ HEADERS_COMPLETE
→ READING_BODY
→ COMPLETE
```

### Rules:

* [ ] Never assume full request is available
* [ ] Store partial data across reads
* [ ] Only process request when COMPLETE

---

## 3. Connection Management

**Goal:** Correct HTTP behavior, not “close everything and pray”

### Must handle:

* [ ] `Connection: keep-alive`
* [ ] `Connection: close`
* [ ] Default behavior (HTTP/1.1 = keep-alive)

### Required behavior:

* [ ] Reuse connection for multiple requests
* [ ] Close only when required

---

## 4. Multiple Requests (Pipeline Handling)

**Goal:** Don’t lose requests like a careless intern

### Must handle:

* [ ] Extract multiple requests from a single buffer
* [ ] Process them sequentially

---

## 5. Timeout + Resource Protection

**Goal:** Prevent your server from being abused

### Must implement:

* [ ] Client inactivity timeout
* [ ] Header timeout (slowloris protection)
* [ ] Max body size limit

### Must handle:

* [ ] Client connects but sends nothing
* [ ] Client sends extremely slow data

---

## 6. Config Enforcement

**Goal:** Config is not decoration

### Must enforce:

* [ ] Allowed HTTP methods per route
* [ ] Max body size
* [ ] Root and location matching
* [ ] Error pages

### Common failure:

* Parsed config but not used everywhere ❌

---

## 7. CGI (Execution Layer)

**Goal:** Works reliably, not “demo works once”

### Must support:

* [ ] Correct environment variables
* [ ] Pipe setup (stdin/stdout)
* [ ] Non-blocking execution
* [ ] Proper cleanup (no zombie processes)

---

## 8. Response Correctness

**Goal:** Valid HTTP responses

### Must include:

* [ ] Correct status line
* [ ] Required headers
* [ ] Correct Content-Length (or chunked)

---

## 9. Large Input Handling

**Goal:** Don’t crash like a toy program

### Must handle:

* [ ] Large request bodies safely
* [ ] Avoid loading everything blindly into memory

---

## 10. Error Handling (Evaluator’s favorite weapon)

### Must correctly respond to:

* [ ] Malformed request
* [ ] Invalid headers
* [ ] Unsupported method
* [ ] Oversized body

---

# 🧠 How to Use This (don’t misuse it)

When you give this to an AI, don’t say:

> “implement webserv”

Say:

> “Check my code against this checklist. Identify missing parts and weak areas. Focus on correctness under partial reads and multiple requests.”

Otherwise the AI will happily hallucinate a perfect server while yours quietly collapses under one bad packet.

---

# ⚠️ Final Reality Check

If your server fails on even ONE of these:

* partial recv
* chunked encoding
* multiple requests

👉 you’re not “almost done”
👉 you’re “looks done, fails evaluation”

---

# 🧠 Synthesis Insight

You’re learning something most devs never internalize:

> Systems don’t fail on what you implemented
> They fail on what you *assumed*

This checklist is basically:

* a map of your assumptions
* and where they will betray you

Fix those, and suddenly your code stops being fragile and starts being… annoyingly correct.
