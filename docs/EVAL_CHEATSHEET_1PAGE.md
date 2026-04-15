# 42 webserv — 1‑Page Eval Cheat Sheet (copy/paste)

Goal: run a fast demo without remembering details.

Prereqs (once):
```sh
make -j
mkdir -p uploads_eval
```

Start server (terminal 1):
```sh
./webserv testing/eval_config.conf
```

All checks (terminal 2):

## A) Multi-site (different interface/port)
```sh
curl -i http://127.0.0.1:8090/ | sed -n '1,8p'
```
Expect: `HTTP/1.1 200`

```sh
curl -i http://127.0.0.2:8091/ | sed -n '1,8p'
```
Expect: `HTTP/1.1 200`

> If `127.0.0.2` fails in evaluator environment: change config to `127.0.0.1:8091`.

## B) Custom 404 page
```sh
curl -i http://127.0.0.1:8090/does-not-exist | sed -n '1,12p'
```
Expect: `HTTP/1.1 404` and body contains your custom HTML from `www1/404.html`.

## C) Route to different directory + index override
```sh
curl -i http://127.0.0.1:8090/site2/ | sed -n '1,12p'
```
Expect: `HTTP/1.1 200` and serves `www2/home.html`.

## D) Client body size limit
Under limit (9 bytes):
```sh
curl -i -X POST -H 'Content-Type: text/plain' --data '123456789' http://127.0.0.1:8090/Upload | sed -n '1,12p'
```
Expect: `HTTP/1.1 201`

Over limit (11 bytes):
```sh
curl -i -X POST -H 'Content-Type: text/plain' --data '12345678901' http://127.0.0.1:8090/Upload | sed -n '1,12p'
```
Expect: `HTTP/1.1 413`

## E) Allowed methods per route (405 + Allow header)
Create file to delete:
```sh
echo 'delete-me test file' > www1/delete_me.txt
```

GET should be rejected:
```sh
curl -i http://127.0.0.1:8090/delete/delete_me.txt | sed -n '1,20p'
```
Expect: `HTTP/1.1 405` and header `Allow: DELETE`

DELETE should succeed:
```sh
curl -i -X DELETE http://127.0.0.1:8090/delete/delete_me.txt | sed -n '1,20p'
```
Expect: `HTTP/1.1 204`

DELETE again (file gone):
```sh
curl -i -X DELETE http://127.0.0.1:8090/delete/delete_me.txt | sed -n '1,20p'
```
Expect: `HTTP/1.1 404`

## F) (Optional) One-command local sanity run
```sh
./scripts/eval_smoke_test.sh
```

---

If something breaks quickly:
- Check ports: `ss -ltnp | grep -E ':8090|:8091'`
- Confirm custom 404 exists under the server root.
- Confirm `uploads_eval/` exists and is writable.
