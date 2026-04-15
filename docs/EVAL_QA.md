# Webserv Evaluation Q&A (quick answers)

Use this when the evaluator asks “why” after your curl demo.

---

## Config / Routing

### “How do you choose which server config to use?”
- By the listening socket that accepted the connection (`host:port`).
- Each `server {}` becomes a `(host,port)` entry.
- If multiple server blocks share the same `(host,port)`, your code should have a deterministic selection rule (often `server_name` in nginx; if you don’t support that, say you select by host/port only).

### “How do you choose the location?”
- Longest-prefix match on the URL path.
- Example: request `/images/png/cat.png` matches `/images/png` over `/images` over `/`.

### “How do root/index work between server and location?”
- Location overrides server:
  - If the location `root` is set, use it; otherwise fallback to server root.
  - If the location `index` is set, use it; otherwise fallback to server index.

---

## Methods

### “How do you restrict methods for a route?”
- Each location stores an allowed method list from config (`methods GET POST;`).
- Before routing, compare request method to that list.
- If not allowed, return `405` and include an `Allow:` header listing allowed methods.

### “What’s the difference between 403 and 405 in your project?”
- `405` = the route exists but the method is not permitted for that location.
- `403` = the server refuses the action even if the method is understood (e.g., trying POST where upload isn’t enabled, or directory request without autoindex/index).

---

## Error pages

### “How do custom error pages work?”
- Server config maps status code → file path (e.g., `error_page 404 /404.html;`).
- When generating an error response, it checks if that file exists under the server root and serves it as `text/html`.

Common gotcha to mention if asked:
- If your implementation builds the path like `server_root + "/404.html"`, the config target should start with `/`.

---

## Body size limit

### “How do you enforce client_max_body_size?”
- The request parser receives a maximum body size.
- If the body exceeds the limit, parsing throws and the engine responds with `413`.

---

## Directory handling

### “What happens if I request a directory?”
- If index file exists → serve it.
- If no index and autoindex is on → generate a directory listing.
- Otherwise → `403`.

---

## Debugging during eval

### “If something doesn’t work, what do you check first?”
- The listening output (did it bind?)
- Port conflicts: `ss -ltnp | grep <port>`
- Curl response headers: `curl -i ...`
- Config paths (relative vs prefix)

---

## If they ask about CGI (bonus-ish)

If your project supports CGI:
- Explain: match location `/cgi-bin`, verify extension in allowed list, fork/exec script, connect pipes, forward output as HTTP response.
- Demo with one known script and show correct status + output.
