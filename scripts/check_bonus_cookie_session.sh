#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PORT="18092"
SERVER_BIN="$ROOT_DIR/webserv"
CONFIG_FILE="$ROOT_DIR/testing/runtime_bonus_cookie_session.conf"
SERVER_LOG="$(mktemp)"

fail() {
	echo "[FAIL] $1"
	echo "--- server log ---"
	cat "$SERVER_LOG"
	exit 1
}

pass() {
	echo "[OK] $1"
}

cleanup() {
	if [[ -n "${SERVER_PID:-}" ]] && kill -0 "$SERVER_PID" 2>/dev/null; then
		kill "$SERVER_PID" 2>/dev/null || true
		wait "$SERVER_PID" 2>/dev/null || true
	fi
	rm -f "$SERVER_LOG"
}

trap cleanup EXIT

if [[ ! -x "$SERVER_BIN" ]]; then
	fail "webserv binary not found. Run: make"
fi

if [[ ! -f "$CONFIG_FILE" ]]; then
	fail "missing config file: $CONFIG_FILE"
fi

"$ROOT_DIR/scripts/free_ports.sh" "$PORT" >/dev/null 2>&1 || true

"$SERVER_BIN" "$CONFIG_FILE" >"$SERVER_LOG" 2>&1 &
SERVER_PID=$!
sleep 1

if ! kill -0 "$SERVER_PID" 2>/dev/null; then
	fail "server failed to start"
fi

request_headers() {
	local path="$1"
	local cookie_header="${2:-}"
	if [[ -n "$cookie_header" ]]; then
		curl -sS -D - -o /dev/null -H "Cookie: $cookie_header" "http://127.0.0.1:${PORT}${path}" | tr -d '\r'
	else
		curl -sS -D - -o /dev/null "http://127.0.0.1:${PORT}${path}" | tr -d '\r'
	fi
}

require_status_200() {
	local headers="$1"
	if ! printf '%s\n' "$headers" | grep -q '^HTTP/1\.1 200 '; then
		fail "expected HTTP 200"
	fi
}

extract_set_cookie_value() {
	local headers="$1"
	printf '%s\n' "$headers" | awk '/^Set-Cookie:/{print $2; exit}' | sed 's/;.*$//'
}

first_headers="$(request_headers '/')"
require_status_200 "$first_headers"
first_sid="$(extract_set_cookie_value "$first_headers")"
if [[ -z "$first_sid" || "$first_sid" != webservsid=* ]]; then
	fail "first request must issue webservsid cookie"
fi
pass "new client receives session cookie"

second_headers="$(request_headers '/' "$first_sid")"
require_status_200 "$second_headers"
if printf '%s\n' "$second_headers" | grep -qi '^Set-Cookie:'; then
	fail "existing valid session should not get another Set-Cookie"
fi
pass "existing static session is reused"

multi_cookie="theme=dark; ${first_sid}; mode=debug"
third_headers="$(request_headers '/' "$multi_cookie")"
require_status_200 "$third_headers"
if printf '%s\n' "$third_headers" | grep -qi '^Set-Cookie:'; then
	fail "multiple-cookie header with valid session should not reissue session"
fi
pass "multiple cookies parse correctly"

cgi_existing_headers="$(request_headers '/cgi-bin/test.py' "$first_sid")"
require_status_200 "$cgi_existing_headers"
if printf '%s\n' "$cgi_existing_headers" | grep -qi '^Set-Cookie:'; then
	fail "cgi request with valid session should not reissue cookie"
fi
pass "cgi request honors existing session"

cgi_new_headers="$(request_headers '/cgi-bin/test.py')"
require_status_200 "$cgi_new_headers"
cgi_sid="$(extract_set_cookie_value "$cgi_new_headers")"
if [[ -z "$cgi_sid" || "$cgi_sid" != webservsid=* ]]; then
	fail "cgi request without session must receive webservsid cookie"
fi
pass "cgi request without session receives cookie"

malformed_headers="$(request_headers '/' '; ; = ; bad ; k')"
require_status_200 "$malformed_headers"
malformed_sid="$(extract_set_cookie_value "$malformed_headers")"
if [[ -z "$malformed_sid" || "$malformed_sid" != webservsid=* ]]; then
	fail "malformed cookie input should still produce a new session cookie"
fi
pass "malformed cookie input is handled safely"

echo "All bonus cookie/session checks passed."
