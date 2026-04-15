# 42 `webserv` Evaluation Runbook (No-Memory Version)

This is a **manual defense script**: copy/paste the commands in order and you’ll hit the common evaluation checklist items.

If you want a printable, minimal version (commands + expected status codes only), use: `docs/EVAL_CHEATSHEET_1PAGE.md`.

> Assumption: you run everything from the repository root (the folder containing `Makefile`).

---

## 0) One-time preparation (do this before evaluation day)

### A. Keep an evaluation config file
- Use: `testing/eval_config.conf`
- It demonstrates: multi-site listen, custom error pages, per-location roots, per-location index, client body size limit, allowed methods.

### B. Make error pages obviously custom
Edit these so you can visually prove which site responded:
- `www1/404.html` contains a unique string like: `CUSTOM 404 (www1)`
- `www2/404.html` contains a different string like: `CUSTOM 404 (www2)`

### C. Create an upload folder
Your config uses `upload_path ./uploads_eval;`.

Make sure it exists and is writable:
```sh
mkdir -p uploads_eval
```

### D. (Optional but recommended) Know what “correct” status codes are
Authoritative list: https://www.iana.org/assignments/http-status-codes/http-status-codes.xhtml

For your demo, you mainly need:
- `200 OK`
- `201 Created`
- `204 No Content`
- `301/302` (if you demo redirects)
- `400 Bad Request`
- `403 Forbidden`
- `404 Not Found`
- `405 Method Not Allowed` (**with `Allow:` header**)
- `413 Content Too Large` (RFC 9110/IANA name)
- `500 Internal Server Error`

> If your project uses a different reason phrase (e.g. “Payload Too Large”), many evaluators don’t care — but some do. Use IANA/RFC naming if you want to be safest.

---

## 1) Evaluation-day quick setup

### A. Build
```sh
make -j
```

### B. Make sure ports are free
This runbook uses ports **8090** and **8091**.

```sh
ss -ltnp | grep -E ':8090|:8091' || true
```

If anything is already listening, stop it or change ports in `testing/eval_config.conf`.

### C. Start the server
```sh
./webserv testing/eval_config.conf
```

You should see something like:
- `Listening on 127.0.0.1:8090`
- `Listening on 127.0.0.2:8091`

If `127.0.0.2` doesn’t work in the evaluator environment, change it to `127.0.0.1:8091`.

---

## 2) The demo checklist (curl commands + expected results)

Open another terminal and run these one by one.

### 2.1 Multiple websites on different interfaces and ports
**Website A**:
```sh
curl -i http://127.0.0.1:8090/
```
Expected:
- `HTTP/1.1 200`
- Body corresponds to `www1/index.html`.

**Website B**:
```sh
curl -i http://127.0.0.2:8091/
```
Expected:
- `HTTP/1.1 200`
- Body corresponds to `www2/home.html`.

---

### 2.2 Default file for directories (index)
```sh
curl -i http://127.0.0.1:8090/
```
Expected:
- Directory `/` serves the server `index` (example: `index.html`).

---

### 2.3 Custom error page (modify 404 and prove it)
```sh
curl -i http://127.0.0.1:8090/this-file-should-not-exist
```
Expected:
- `HTTP/1.1 404`
- Body contains your custom text from `www1/404.html`.

---

### 2.4 Routes to different directories (+ index override)
This config routes `/site2` to `www2`.

```sh
curl -i http://127.0.0.1:8090/site2/
```
Expected:
- `HTTP/1.1 200`
- Body is `www2/home.html` (location-level `root` + `index` override).

---

### 2.5 Limit the client request body size (413)
Your server A sets `client_max_body_size 10;`.

Under the limit (9 bytes):
```sh
curl -i -X POST -H 'Content-Type: text/plain' --data '123456789' http://127.0.0.1:8090/Upload
```
Expected:
- `HTTP/1.1 201` (Created)

Over the limit (11 bytes):
```sh
curl -i -X POST -H 'Content-Type: text/plain' --data '12345678901' http://127.0.0.1:8090/Upload
```
Expected:
- `HTTP/1.1 413`

Reason phrase note:
- IANA/RFC name is `413 Content Too Large`.
- If your server prints `413 Payload Too Large`, that’s a common older phrase; some evaluators don’t care, but if they are strict you can update the mapping in `src/engine/engine_response.cpp` (the `reasonPhraseForStatusCodeEngine()` switch).

---

### 2.6 Allowed methods per route (405 + Allow header)
This config allows only `DELETE` under `/delete`.

Create a file to delete:
```sh
echo 'delete-me test file' > www1/delete_me.txt
```

Try GET (should be rejected):
```sh
curl -i http://127.0.0.1:8090/delete/delete_me.txt
```
Expected:
- `HTTP/1.1 405`
- Response headers include `Allow: DELETE`

Now DELETE (should succeed):
```sh
curl -i -X DELETE http://127.0.0.1:8090/delete/delete_me.txt
```
Expected:
- `HTTP/1.1 204`

Try deleting again (should be missing):
```sh
curl -i -X DELETE http://127.0.0.1:8090/delete/delete_me.txt
```
Expected:
- `HTTP/1.1 404`

---

## 3) What to say (short verbal explanation)

Keep it to ~30 seconds:

1) **Server selection**: “Each `server {}` gives a `listen host:port`; the engine creates/binds a socket per unique `(host,port)`.”
2) **Location match**: “For each request path, I choose the best matching location using the longest prefix match.”
3) **Path resolution**: “Location `root/index` override server `root/index`. For directories I try `index`; if missing and autoindex is on, I generate a listing; otherwise return 403/404.”
4) **Method enforcement**: “Before routing, I check location allowed methods; if not allowed I return 405 and include the `Allow:` header.”
5) **Body limit**: “Parser enforces `client_max_body_size`; if exceeded I return 413.”

---

## 4) Troubleshooting

### Ports already in use
```sh
ss -ltnp | grep -E ':8090|:8091'
```
Change ports in `testing/eval_config.conf` or kill the other process.

### Server won’t bind to 127.0.0.2
Edit config to use `127.0.0.1:8091` instead.

### Custom error page not served
Common causes:
- `error_page 404 404.html;` (missing leading `/`) → your code concatenates as `root + target`.
- File doesn’t exist under the server root.

### 405 missing Allow header
Evaluator may expect `Allow:`. Verify with:
```sh
curl -i http://127.0.0.1:8090/delete/delete_me.txt
```

### Body limit not triggering
Make sure the server you’re hitting has `client_max_body_size` set low, and that your `--data` is actually longer than the limit.

---

## 5) Optional: automated smoke test
If you want, you can run:
```sh
scripts/eval_smoke_test.sh
```
It starts the server, runs the same curl checks, and stops it.
