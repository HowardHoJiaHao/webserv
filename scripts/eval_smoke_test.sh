#!/usr/bin/env sh
# Minimal smoke test for the evaluation demo config.
# Runs the same checks as docs/EVAL_RUNBOOK.md.

set -eu

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$ROOT_DIR"

CONF="testing/eval_config.conf"

if [ ! -f "$CONF" ]; then
  echo "Missing $CONF" >&2
  exit 1
fi

if [ ! -x "./webserv" ]; then
  # During evaluation you may build manually, but for local prep this is convenient.
  echo "./webserv not found; running 'make -j'..." >&2
  make -j
  if [ ! -x "./webserv" ]; then
    echo "Build did not produce ./webserv" >&2
    exit 1
  fi
fi

mkdir -p uploads_eval

# Kill any stray webserv instances to avoid port conflicts.
pkill -f "(^|/)webserv(\\s|$)" 2>/dev/null || true

./webserv "$CONF" >/tmp/ws_eval_smoke_server.log 2>&1 &
PID=$!

sleep 0.5

echo "== Multi-site =="
curl -sS -i http://127.0.0.1:8090/ | sed -n '1,12p'
echo
curl -sS -i http://127.0.0.2:8091/ | sed -n '1,12p' || echo "(127.0.0.2 failed; change to 127.0.0.1:8091 if needed)"
echo

echo "== Custom 404 =="
curl -sS -i http://127.0.0.1:8090/does-not-exist | sed -n '1,12p'
echo

echo "== /site2 index override =="
curl -sS -i http://127.0.0.1:8090/site2/ | sed -n '1,12p'
echo

echo "== Body limit =="
curl -sS -i -X POST -H 'Content-Type: text/plain' --data '123456789' http://127.0.0.1:8090/Upload | sed -n '1,12p'
echo
curl -sS -i -X POST -H 'Content-Type: text/plain' --data '12345678901' http://127.0.0.1:8090/Upload | sed -n '1,12p'
echo

echo "== Methods (405 + Allow) / DELETE =="
echo 'delete-me test file' > www1/delete_me.txt
curl -sS -i http://127.0.0.1:8090/delete/delete_me.txt | sed -n '1,20p'
echo
curl -sS -i -X DELETE http://127.0.0.1:8090/delete/delete_me.txt | sed -n '1,20p'
echo
curl -sS -i -X DELETE http://127.0.0.1:8090/delete/delete_me.txt | sed -n '1,20p'
echo

kill "$PID" 2>/dev/null || true
wait "$PID" 2>/dev/null || true

echo "Server log: /tmp/ws_eval_smoke_server.log"
