# 🧠 CGI Integration Guide (Teammate Handover)

## 🧩 Current State (READ FIRST)

The system already has a **fully working CGI execution engine**.

### ✅ Already implemented (DO NOT TOUCH)

* `launchCGI()` → fork + exec + pipes
* `Connection::CGIContext`
* `Connection::CGI_RUNNING` state
* `select()` integration for:

  * CGI stdout (read)
  * CGI stdin (write)
* Non-blocking I/O
* Data flow:

  ```
  CGI → pipe → server → writeBuffer → client
  ```
* Lifecycle:

  ```
  fork → exec → read → EOF → waitpid → cleanup
  ```

👉 This part is COMPLETE. Do NOT rewrite it.
👉 Your job is to **connect it to HTTP routing**.

---

# 🎯 Goal

Make CGI work like this:

```
Client request
→ routeRequest()
→ detect CGI request
→ launchCGI()
→ existing pipeline handles everything
→ response returned to client
```

---

# 🧱 PHASE 1 — Detect CGI Request

## 📍 File: routeRequest(...)

Add logic to detect whether a request should use CGI.

### Minimal version:

```cpp
bool isCGIRequest(const HttpRequest& request)
{
    std::string path = request.getPath();

    // prefix-based
    if (path.find("/cgi") == 0)
        return true;

    // extension-based
    if (path.find(".py") != std::string::npos)
        return true;

    return false;
}
```

---

# 🧱 PHASE 2 — Call launchCGI()

Inside `routeRequest(...)`:

```cpp
if (isCGIRequest(request))
{
    launchCGI(conn, request);

    // IMPORTANT:
    // Do NOT build response here
    // Let event loop handle it

    return "";
}
```

## ⚠️ Integration Contract (IMPORTANT)

In your current codebase, `routeRequest(...)` does **not** receive `Connection*`.

So you must choose **one** of these architectures and keep it consistent:

### Option A (recommended minimal change)

Trigger CGI in `handleClientRequest(...)` (where `Connection* conn` already exists), **before** calling `routeRequest(...)`.

Flow:

```
parse request
→ if CGI request: launchCGI(conn, request), return from handler
→ else: normal routeRequest(...)
```

### Option B

Change `routeRequest(...)` signature to accept `Connection*` and call `launchCGI(conn, request)` there.

If you pick this, update declaration + all call sites.

👉 Do **not** keep mixed behavior.

---

# 🧠 Important Behavior

After calling `launchCGI()`:

```
state → CGI_RUNNING
```

Then automatically:

```
processIncomingData() → read CGI output
processOutgoingData() → send to client
```

👉 You DO NOTHING else here.

---

# 🧱 PHASE 3 — Execute Correct Script

## ❌ Current (placeholder)

```cpp
char* argv[] = { (char*)"/bin/ls", NULL };
execve("/bin/ls", argv, envp);
```

## ✅ Replace with script execution

```cpp
std::string script = resolveScriptPath(request.getPath());

char* argv[] = {
    const_cast<char*>(script.c_str()),
    NULL
};

execve(script.c_str(), argv, envp);
```

### 🧠 Example mapping

```
/cgi-bin/test.py → ./www/cgi-bin/test.py
```

## ✅ Required path policy (add this explicitly)

Before executing script:

1. Reject paths containing `..` (return `403`)
2. Restrict CGI to trusted prefix (e.g. `/cgi-bin/`)
3. Resolve full path from selected server root
4. Check file exists (`404` if not)
5. Check execute permission (`403` if not executable)
6. Restrict extensions (example: `.py`, `.pl`) unless intentionally broader

This prevents accidental arbitrary process execution.

---

# 🧱 PHASE 4 — (Optional) Environment Variables

```cpp
std::vector<std::string> env;

env.push_back("REQUEST_METHOD=" + request.getMethod());
env.push_back("QUERY_STRING=" + request.getQuery());
env.push_back("CONTENT_LENGTH=" + toString(request.getBody().size()));

char* envp[env.size() + 1];
for (size_t i = 0; i < env.size(); i++)
    envp[i] = const_cast<char*>(env[i].c_str());

envp[env.size()] = NULL;
```

---

# 🧱 PHASE 5 — CGI Output Expectations

## ⚠️ VERY IMPORTANT

The server currently forwards CGI stdout directly to client.

That means CGI output must be a valid HTTP-style payload that client can parse.

Minimum practical contract:

```
Content-Type: text/html\r\n
\r\n
<html>Hello</html>
```

Better contract (recommended): include status header too.

```
Status: 200 OK\r\n
Content-Type: text/html\r\n
\r\n
<html>Hello</html>
```

If you later decide server should wrap CGI output into full `HTTP/1.1 ...`, document that separately and keep behavior consistent.

## ✅ POST body ownership contract

When CGI is launched, ensure request body source is clear:

* If writing CGI stdin from `conn->getReadBuffer()`, copy request body there intentionally.
* Do not overwrite/clear it before CGI stdin is fully written.
* Close CGI stdin after full body write, then continue reading CGI stdout.

Without this contract, POST CGI may hang or get empty stdin.

---

# 🧱 PHASE 5.5 — Child Process Robustness

Add these rules so CGI cannot hang server forever:

1. Track CGI start time.
2. If CGI exceeds timeout (example 5–10s), `kill(pid, SIGKILL)`.
3. Always `waitpid` after child exit/kill to avoid zombies.
4. On timeout/failure, return deterministic error (e.g. `504` or `500`).
5. Apply max CGI output size guard to prevent unbounded memory growth.

This closes the biggest robustness gap.

---

# 🧱 PHASE 6 — Test Checklist (Reproducible)

## ✅ Add a real fixture script

Create `./www1/cgi-bin/test.py`:

```
#!/usr/bin/env python3
import os
import sys

body = sys.stdin.read()
print("Content-Type: text/plain")
print()
print("method=" + os.environ.get("REQUEST_METHOD", ""))
print("body=" + body)
print("query=" + os.environ.get("QUERY_STRING", ""))
```

Then ensure executable:

```
chmod +x ./www1/cgi-bin/test.py
```

## ✅ Basic tests

```
GET /cgi-bin/test.py
→ response body contains method=GET
```

```
POST /cgi-bin/test.py (body=hello)
→ response body contains method=POST and body=hello
```

```
GET /cgi-bin/notfound.py
→ 404
```

```
GET /cgi-bin/../../etc/passwd
→ 403
```

```
non-executable script
→ 403 (or configured failure code, but must be consistent)
```

## 🔍 Debug tips

1. Confirm `launchCGI()` is called
2. Log resolved script absolute path
3. Log child PID and timeout decision
4. Print first bytes of CGI stdout into server log
5. Ensure script executable + correct shebang

---

# 🧱 PHASE 7 — Decision Table (quick implementation reference)

| Condition | Expected Action |
| --- | --- |
| CGI route matched | `launchCGI(conn, request)` and return to event loop |
| Script missing | return `404` |
| Script not executable | return `403` |
| Path traversal attempt | return `403` |
| CGI timeout | kill child, return `500/504` |
| CGI completes | stdout forwarded to client |

---

# 🧱 PHASE 8 — Definition of Done

Mark CGI integration complete only if all are true:

* [ ] CGI requests are detected reliably.
* [ ] `launchCGI()` is actually called from real request path.
* [ ] Placeholder `/bin/ls` replaced by resolved script execution.
* [ ] GET and POST CGI both work.
* [ ] Failure paths return consistent status codes.
* [ ] Child process timeout + cleanup verified.
* [ ] No blocking behavior introduced.

---

# 🧠 Architecture Summary

## Engine (already done)

* pipes
* select loop
* process handling
* data flow

## Teammate responsibility

* routing decision
* script selection
* environment setup

---

# 🚨 Common Mistakes

❌ Rewriting CGI engine
❌ Blocking calls
❌ Mixing HTTP response logic
❌ Wrong script path
❌ Missing execute permission
❌ Forgetting `Connection*` integration point (routeRequest signature mismatch)
❌ Forgetting timeout/kill/waitpid cleanup for hung CGI

---

# 🧠 Final Mental Model

```
routeRequest()
    ↓
detect CGI
    ↓
launchCGI()
    ↓
[event loop takes over]
    ↓
response sent
```

---

# 🧾 Final Status

| Component        | Status         |
| ---------------- | -------------- |
| CGI engine       | ✅ DONE         |
| Routing          | ❌ TO IMPLEMENT |
| Script execution | ❌ TO IMPLEMENT |
| Env setup        | ⚠️ OPTIONAL    |

---

# 🧠 Final Note

You are NOT building CGI.

You are just:

> deciding WHEN to use it

The hard part is already done.
