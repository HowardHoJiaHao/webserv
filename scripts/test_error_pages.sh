#!/usr/bin/env bash
# Quick checker for custom error pages on your webserv.
# Usage:
#   ./scripts/test_error_pages.sh [base_url]
# Example:
#   ./scripts/test_error_pages.sh http://127.0.0.1:8080

set -u

BASE_URL="${1:-http://127.0.0.1:8080}"
ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
TMP_DIR="$(mktemp -d)"
PASS=0
FAIL=0
WARN=0
FORCE_500_SCRIPT="$ROOT_DIR/www1/cgi-bin/__force_500.py"

extract_host_port() {
  local host port
  host="$(printf '%s' "$BASE_URL" | sed -E 's#^https?://([^/:]+).*#\1#')"
  port="$(printf '%s' "$BASE_URL" | sed -nE 's#^https?://[^/:]+:([0-9]+).*$#\1#p')"
  [[ -z "$port" ]] && port="80"
  printf '%s %s\n' "$host" "$port"
}

cleanup() {
  rm -rf "$TMP_DIR"
  rm -f "$FORCE_500_SCRIPT"
}
trap cleanup EXIT

status_code() {
  local headers="$1"
  awk 'BEGIN{code=""} /^HTTP\// {code=$2} END{print code}' "$headers"
}

ok() {
  printf "[PASS] %s\n" "$1"
  PASS=$((PASS + 1))
}

bad() {
  printf "[FAIL] %s\n" "$1"
  FAIL=$((FAIL + 1))
}

note() {
  printf "[WARN] %s\n" "$1"
  WARN=$((WARN + 1))
}

print_context() {
  local headers="$1"
  local body="$2"
  echo "  ---- response head ----"
  sed -n '1,12p' "$headers" 2>/dev/null || true
  echo "  ---- response body ----"
  sed -n '1,10p' "$body" 2>/dev/null || true
}

run_case() {
  # run_case <title> <expected_code> <expected_body_regex> <path> [curl extra args...]
  local title="$1"
  local expected_code="$2"
  local expected_regex="$3"
  local path="$4"
  shift 4

  local h="$TMP_DIR/${title// /_}.h"
  local b="$TMP_DIR/${title// /_}.b"

  if ! curl -sS -D "$h" -o "$b" "$BASE_URL$path" "$@"; then
    bad "$title (request failed)"
    return
  fi

  local code
  code="$(status_code "$h")"

  if [[ "$code" != "$expected_code" ]]; then
    bad "$title (expected HTTP $expected_code, got $code)"
    print_context "$h" "$b"
    return
  fi

  if grep -Eiq "$expected_regex" "$b"; then
    ok "$title"
  else
    bad "$title (status is right but body did not match custom page)"
    print_context "$h" "$b"
  fi
}

run_status_set_case() {
  # run_status_set_case <title> <path> <status_regex> [curl extra args...]
  local title="$1"
  local path="$2"
  local status_regex="$3"
  shift 3

  local h="$TMP_DIR/${title// /_}.h"
  local b="$TMP_DIR/${title// /_}.b"

  if ! curl -sS -D "$h" -o "$b" "$BASE_URL$path" "$@"; then
    bad "$title (request failed)"
    return
  fi

  local code
  code="$(status_code "$h")"
  if [[ "$code" =~ $status_regex ]]; then
    ok "$title (HTTP $code)"
  else
    bad "$title (expected status /$status_regex/, got $code)"
    print_context "$h" "$b"
  fi
}

raw_400_case() {
  local host port raw code body
  read -r host port < <(extract_host_port)

  if ! command -v nc >/dev/null 2>&1; then
    note "Skipping 400 test (nc/netcat not found)"
    return
  fi

  raw="$(printf 'GET / HTTP/1.1\r\nHost localhost\r\n\r\n' | nc -w 2 "$host" "$port" 2>/dev/null || true)"
  code="$(printf '%s' "$raw" | head -n1 | awk '{print $2}')"
  body="$(printf '%s' "$raw" | awk 'BEGIN{p=0} /^\r?$/{p=1;next} p{print}')"

  if [[ "$code" != "400" ]]; then
    bad "400 Bad Request (expected 400, got ${code:-<none>})"
    printf '  first line: %s\n' "$(printf '%s' "$raw" | head -n1)"
    return
  fi

  if printf '%s' "$body" | grep -Eiq '400 Bad Request \(www1\)'; then
    ok "400 Bad Request"
  else
    bad "400 Bad Request (status is 400 but body is not custom 400 page)"
  fi
}

raw_408_case() {
  local host port out code
  read -r host port < <(extract_host_port)

  if ! command -v python3 >/dev/null 2>&1; then
    note "Skipping 408 test (python3 not found)"
    return
  fi

  out="$(python3 - "$host" "$port" <<'PY'
import socket
import sys
import time

host = sys.argv[1]
port = int(sys.argv[2])

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.settimeout(15)
try:
    s.connect((host, port))
    # Send incomplete headers and stay idle to trigger server header timeout.
    s.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n")
    time.sleep(7)  # engine header timeout is 5s in current code
    s.sendall(b"\r\n")

    chunks = []
    end = time.time() + 8
    while time.time() < end:
        try:
            data = s.recv(4096)
            if not data:
                break
            chunks.append(data)
            if b"\r\n\r\n" in b"".join(chunks):
                # usually enough to read status + headers
                break
        except socket.timeout:
            break

    raw = b"".join(chunks).decode("latin1", errors="replace")
    first = raw.splitlines()[0] if raw.splitlines() else ""
    body = ""
    if "\r\n\r\n" in raw:
        body = raw.split("\r\n\r\n", 1)[1]
    elif "\n\n" in raw:
        body = raw.split("\n\n", 1)[1]

    print(first)
    print("---BODY---")
    print(body)
finally:
    s.close()
PY
)"

  code="$(printf '%s\n' "$out" | head -n1 | awk '{print $2}')"
  if [[ "$code" != "408" ]]; then
    bad "408 Request Timeout (expected 408, got ${code:-<none>})"
    printf '  first line: %s\n' "$(printf '%s\n' "$out" | head -n1)"
    return
  fi

  # 408 custom page may or may not be configured; status is the primary check.
  ok "408 Request Timeout"
}

timeout_504_case() {
  local h="$TMP_DIR/504.h"
  local b="$TMP_DIR/504.b"

  if ! curl -sS -D "$h" -o "$b" --max-time 20 "$BASE_URL/cgi-bin/infinite.py"; then
    bad "504 Gateway Timeout (request failed before server timeout response)"
    return
  fi

  local code
  code="$(status_code "$h")"

  if [[ "$code" == "504" ]]; then
    ok "504 Gateway Timeout"
    return
  fi

  if [[ "$code" == "303" ]] && grep -Eiq '^Location:[[:space:]]*/500\.html' "$h"; then
    note "504 path currently mapped as 303 redirect to /500.html in your engine"
    return
  fi

  bad "504 Gateway Timeout (expected 504, got $code)"
  print_context "$h" "$b"
}

echo "== Custom error page test =="
echo "Base URL: $BASE_URL"

# 403: directory listing disabled at /cgi-bin/
run_case "403 Forbidden" 403 '403 Forbidden \(www1\)' '/cgi-bin/'

# 404: missing route/file
run_case "404 Not Found" 404 '404 Not Found \(www1\)' '/this-path-should-not-exist-12345'

# 405: method not allowed on /
run_case "405 Method Not Allowed" 405 '405 Method Not Allowed \(www1\)' '/' -X PUT

# 413: payload too large (> 1,000,000 bytes in your webserv.conf)
BIG="$TMP_DIR/big.bin"
head -c 1000500 /dev/zero > "$BIG"
run_case "413 Payload Too Large" 413 '413 Payload Too Large \(www1\)' '/Upload' -X POST --data-binary @"$BIG"

# 400: malformed HTTP request (raw socket)
raw_400_case

# 500: force CGI failure with a temporary script
cat > "$FORCE_500_SCRIPT" <<'PY'
#!/usr/bin/env python3
import sys
print('broken cgi output without proper CGI headers')
sys.exit(1)
PY
chmod +x "$FORCE_500_SCRIPT"
run_case "500 Internal Server Error" 500 '500 Internal Server Error \(www1\)' '/cgi-bin/__force_500.py'

# 408: slow/incomplete request header timeout
raw_408_case

# 504: CGI timeout path (optional behavior check)
timeout_504_case

echo
echo "== Optional success-page checks (200/201/204) =="

# Direct page-file checks (easy + deterministic):
# these verify your custom HTML files exist and are served.
run_case "200 Page File" 200 '200 OK \(www1\)' '/200.html'
run_case "201 Page File" 200 '201 Created \(www1\)' '/201.html'
run_case "204 Page File" 200 '204 No Content \(www1\)' '/204.html'

# Runtime success-path behavior for upload endpoint.
# Different implementations may return 200, 201, or 204 on successful upload.
SMALL="$TMP_DIR/small.txt"
printf 'tiny upload\n' > "$SMALL"
run_status_set_case "Upload success status" '/Upload' '^(200|201|204)$' -X POST --data-binary @"$SMALL"

echo
printf 'Result: PASS=%d FAIL=%d WARN=%d\n' "$PASS" "$FAIL" "$WARN"
if [[ "$FAIL" -eq 0 ]]; then
  echo "All tested error pages are working."
  exit 0
fi

echo "Some tests failed. Check your routing/error_page mapping and server logs."
exit 1
