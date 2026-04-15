#!/usr/bin/env bash
# Pretty PASS/FAIL smoke test for 42 webserv eval demo config.
# Similar spirit to min_eval_cookie_cgi.sh, but covers broader eval points.

set -u

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$ROOT_DIR" || exit 1

CONF="./eval_config.conf"
SERVER_LOG="/tmp/ws_eval_pretty_server.log"
TMP_DIR="$(mktemp -d)"

pass=0
fail=0
warn=0
CASE_NO=0
SERVER_PID=""

cleanup() {
  if [[ -n "$SERVER_PID" ]]; then
    kill "$SERVER_PID" >/dev/null 2>&1 || true
    wait "$SERVER_PID" >/dev/null 2>&1 || true
  fi
  rm -rf "$TMP_DIR"
}
trap cleanup EXIT

ok() {
  printf "[PASS] %s\n" "$1"
  pass=$((pass + 1))
}

bad() {
  printf "[FAIL] %s\n" "$1"
  fail=$((fail + 1))
}

note() {
  printf "[WARN] %s\n" "$1"
  warn=$((warn + 1))
}

status_code() {
  local headers="$1"
  awk 'BEGIN{code=""} /^HTTP\// {code=$2} END{print code}' "$headers"
}

header_has() {
  local headers="$1"
  local regex="$2"
  grep -Eiq "^${regex}" "$headers"
}

body_has() {
  local body="$1"
  local regex="$2"
  grep -Eq "$regex" "$body"
}

run_req() {
  # Usage: run_req <method> <url> <headers_file> <body_file> [curl extra args...]
  local method="$1"
  local url="$2"
  local headers_file="$3"
  local body_file="$4"
  shift 4
  curl -sS -D "$headers_file" -o "$body_file" -X "$method" "$url" "$@"
}

case_title() {
  CASE_NO=$((CASE_NO + 1))
  printf "\n== [%02d] %s ==\n" "$CASE_NO" "$1"
}

print_fail_context() {
  local headers="$1"
  local body="$2"
  echo "  ---- response head ----"
  sed -n '1,12p' "$headers" 2>/dev/null || true
  echo "  ---- response body ----"
  sed -n '1,8p' "$body" 2>/dev/null || true
}

# Preconditions
if [[ ! -f "$CONF" ]]; then
  echo "Missing $CONF" >&2
  exit 1
fi

if [[ ! -x "./webserv" ]]; then
  echo "./webserv not found; running make -j" >&2
  make -j || exit 1
fi

mkdir -p uploads_eval

# Avoid port conflicts
pkill -f '(^|/)webserv(\s|$)' >/dev/null 2>&1 || true

./webserv "$CONF" >"$SERVER_LOG" 2>&1 &
SERVER_PID=$!
sleep 0.5

BASE_A="http://127.0.0.1:8090"
BASE_B="http://127.0.0.2:8091"

# 1) Multi-site A
case_title "Multi-site A responds"
H="$TMP_DIR/1.h"; B="$TMP_DIR/1.b"
if run_req GET "$BASE_A/" "$H" "$B"; then
  code="$(status_code "$H")"
  if [[ "$code" == "200" ]]; then
    ok "GET / on 127.0.0.1:8090 -> 200"
  else
    bad "Expected 200, got $code"
    print_fail_context "$H" "$B"
  fi
else
  bad "Failed to connect to 127.0.0.1:8090"
fi

# 2) Multi-site B
case_title "Multi-site B responds"
H="$TMP_DIR/2.h"; B="$TMP_DIR/2.b"
if run_req GET "$BASE_B/" "$H" "$B"; then
  code="$(status_code "$H")"
  if [[ "$code" == "200" ]]; then
    ok "GET / on 127.0.0.2:8091 -> 200"
  else
    bad "Expected 200, got $code"
    print_fail_context "$H" "$B"
  fi
else
  note "127.0.0.2:8091 unreachable here (use 127.0.0.1:8091 if needed)"
fi

# 3) Custom 404
case_title "Custom 404"
H="$TMP_DIR/3.h"; B="$TMP_DIR/3.b"
if run_req GET "$BASE_A/does-not-exist" "$H" "$B"; then
  code="$(status_code "$H")"
  if [[ "$code" == "404" ]]; then
    ok "Non-existing path returns 404"
  else
    bad "Expected 404, got $code"
    print_fail_context "$H" "$B"
  fi
else
  bad "Request failed for custom 404 test"
fi

# 4) /site2 override
case_title "Location root/index override"
H="$TMP_DIR/4.h"; B="$TMP_DIR/4.b"
if run_req GET "$BASE_A/site2/" "$H" "$B"; then
  code="$(status_code "$H")"
  if [[ "$code" == "200" ]]; then
    ok "/site2/ responds 200"
  else
    bad "Expected 200, got $code"
    print_fail_context "$H" "$B"
  fi
else
  bad "Request failed for /site2/"
fi

# 5) Body limit
case_title "client_max_body_size enforcement"
H="$TMP_DIR/5a.h"; B="$TMP_DIR/5a.b"
if run_req POST "$BASE_A/Upload" "$H" "$B" -H 'Content-Type: text/plain' --data '123456789'; then
  code="$(status_code "$H")"
  if [[ "$code" != "413" ]]; then
    ok "9-byte POST is accepted (not 413)"
  else
    bad "9-byte POST unexpectedly got 413"
    print_fail_context "$H" "$B"
  fi
else
  bad "9-byte POST request failed"
fi

H="$TMP_DIR/5b.h"; B="$TMP_DIR/5b.b"
if run_req POST "$BASE_A/Upload" "$H" "$B" -H 'Content-Type: text/plain' --data '12345678901'; then
  code="$(status_code "$H")"
  if [[ "$code" == "413" ]]; then
    ok "11-byte POST rejected with 413"
  else
    bad "Expected 413 for oversized body, got $code"
    print_fail_context "$H" "$B"
  fi
else
  bad "11-byte POST request failed"
fi

# 6) Upload then retrieve
case_title "Upload then retrieve"
printf '123456789' > "$TMP_DIR/from_eval.txt"
H="$TMP_DIR/6a.h"; B="$TMP_DIR/6a.b"
if run_req POST "$BASE_A/Upload" "$H" "$B" -H 'Content-Type: text/plain' -H 'Content-Disposition: form-data; name="file"; filename="from_eval.txt"' --data-binary @"$TMP_DIR/from_eval.txt"; then
  code="$(status_code "$H")"
  if [[ "$code" == "200" || "$code" == "201" || "$code" == "204" ]]; then
    ok "Upload endpoint accepted file (HTTP $code)"
  else
    bad "Upload failed with HTTP $code"
    print_fail_context "$H" "$B"
  fi
else
  bad "Upload request failed"
fi

H="$TMP_DIR/6b.h"; B="$TMP_DIR/6b.b"
if run_req GET "$BASE_A/uploads/from_eval.txt" "$H" "$B"; then
  code="$(status_code "$H")"
  if [[ "$code" == "200" ]] && body_has "$B" '123456789'; then
    ok "Uploaded file is retrievable"
  else
    bad "Uploaded file retrieval failed (HTTP $code or wrong content)"
    print_fail_context "$H" "$B"
  fi
else
  bad "Retrieve uploaded file request failed"
fi

# 7) Methods + Allow + DELETE
case_title "Method enforcement and DELETE"
echo 'delete-me test file' > www1/delete_me.txt
H="$TMP_DIR/7a.h"; B="$TMP_DIR/7a.b"
if run_req GET "$BASE_A/delete/delete_me.txt" "$H" "$B"; then
  code="$(status_code "$H")"
  if [[ "$code" == "405" ]] && header_has "$H" 'Allow:[[:space:]].*DELETE'; then
    ok "GET on DELETE-only location -> 405 + Allow"
  else
    bad "Expected 405 + Allow: DELETE, got HTTP $code"
    print_fail_context "$H" "$B"
  fi
else
  bad "GET /delete request failed"
fi

H="$TMP_DIR/7b.h"; B="$TMP_DIR/7b.b"
if run_req DELETE "$BASE_A/delete/delete_me.txt" "$H" "$B"; then
  code="$(status_code "$H")"
  if [[ "$code" == "200" || "$code" == "204" ]]; then
    ok "DELETE existing file succeeded (HTTP $code)"
  else
    bad "Expected 200/204 on first DELETE, got $code"
    print_fail_context "$H" "$B"
  fi
else
  bad "DELETE request failed"
fi

H="$TMP_DIR/7c.h"; B="$TMP_DIR/7c.b"
if run_req DELETE "$BASE_A/delete/delete_me.txt" "$H" "$B"; then
  code="$(status_code "$H")"
  if [[ "$code" == "404" ]]; then
    ok "Second DELETE on missing file -> 404"
  else
    bad "Expected 404 on second DELETE, got $code"
    print_fail_context "$H" "$B"
  fi
else
  bad "Second DELETE request failed"
fi

# 8) Unknown method resilience
case_title "Unknown method does not crash server"
H="$TMP_DIR/8a.h"; B="$TMP_DIR/8a.b"
if run_req BREW "$BASE_A/" "$H" "$B"; then
  code="$(status_code "$H")"
  if [[ "$code" == "400" || "$code" == "405" ]]; then
    ok "Unknown method handled safely (HTTP $code)"
  else
    bad "Expected 400/405 for unknown method, got $code"
    print_fail_context "$H" "$B"
  fi
else
  bad "Unknown method request failed"
fi

H="$TMP_DIR/8b.h"; B="$TMP_DIR/8b.b"
if run_req GET "$BASE_A/" "$H" "$B"; then
  code="$(status_code "$H")"
  if [[ "$code" == "200" ]]; then
    ok "Server still responsive after unknown method"
  else
    bad "Server unhealthy after unknown method (HTTP $code)"
    print_fail_context "$H" "$B"
  fi
else
  bad "Health check after unknown method failed"
fi

# 9) CGI GET/POST
case_title "CGI GET + POST"
H="$TMP_DIR/9a.h"; B="$TMP_DIR/9a.b"
if run_req GET "$BASE_A/cgi-bin/test.py" "$H" "$B"; then
  code="$(status_code "$H")"
  if [[ "$code" == "200" ]] && body_has "$B" '^METHOD=GET'; then
    ok "CGI GET works"
  else
    bad "CGI GET failed (HTTP $code or output mismatch)"
    print_fail_context "$H" "$B"
  fi
else
  bad "CGI GET request failed"
fi

H="$TMP_DIR/9b.h"; B="$TMP_DIR/9b.b"
if run_req POST "$BASE_A/cgi-bin/test.py" "$H" "$B" -H 'Content-Type: text/plain' --data 'hello-cgi'; then
  code="$(status_code "$H")"
  if [[ "$code" == "200" ]] && body_has "$B" 'BODY=hello-cgi'; then
    ok "CGI POST works"
  else
    bad "CGI POST failed (HTTP $code or body mismatch)"
    print_fail_context "$H" "$B"
  fi
else
  bad "CGI POST request failed"
fi

# 10) CGI faulty script timeout handling
case_title "CGI timeout handling"
H="$TMP_DIR/10a.h"; B="$TMP_DIR/10a.b"
if run_req GET "$BASE_A/cgi-bin/infinite.py" "$H" "$B" --max-time 15; then
  code="$(status_code "$H")"
  if [[ "$code" == "500" || "$code" == "504" ]]; then
    ok "Faulty CGI handled with server error (HTTP $code)"
  elif [[ "$code" == "303" ]] && header_has "$H" 'Location:[[:space:]]*/500\.html'; then
    ok "Faulty CGI handled via redirect to error page (HTTP 303 -> /500.html)"
  else
    bad "Expected 500/504 (or 303 -> /500.html), got $code"
    print_fail_context "$H" "$B"
  fi
else
  bad "Faulty CGI request failed (curl timeout or transport error)"
fi

H="$TMP_DIR/10b.h"; B="$TMP_DIR/10b.b"
if run_req GET "$BASE_A/" "$H" "$B"; then
  code="$(status_code "$H")"
  if [[ "$code" == "200" ]]; then
    ok "Server still responsive after faulty CGI"
  else
    bad "Server unhealthy after faulty CGI (HTTP $code)"
    print_fail_context "$H" "$B"
  fi
else
  bad "Post-CGI health check failed"
fi

echo
printf "Result: PASS=%d FAIL=%d WARN=%d\n" "$pass" "$fail" "$warn"
if [[ "$fail" -eq 0 ]]; then
  echo "Overall: SMOKE CHECK PASSED"
  echo "Server log: $SERVER_LOG"
  exit 0
fi

echo "Overall: NEEDS FIX BEFORE EVAL"
echo "Server log: $SERVER_LOG"
exit 1
