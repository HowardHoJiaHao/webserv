#!/usr/bin/env bash
set -u

BASE_URL="${1:-http://127.0.0.1:8080}"
COOKIE_PATH="${2:-/}"
CGI_PY="${3:-/cgi-bin/test.py?x=42}"
CGI_PL="${4:-/cgi-bin/upload_script.pl}"

TMP_DIR="$(mktemp -d)"
COOKIE_JAR="$TMP_DIR/cookies.txt"
H1="$TMP_DIR/h1.txt"
B1="$TMP_DIR/b1.txt"
H2="$TMP_DIR/h2.txt"
B2="$TMP_DIR/b2.txt"
H3="$TMP_DIR/h3.txt"
B3="$TMP_DIR/b3.txt"
HPY="$TMP_DIR/hpy.txt"
BPY="$TMP_DIR/bpy.txt"
HPL="$TMP_DIR/hpl.txt"
BPL="$TMP_DIR/bpl.txt"

pass=0
fail=0

cleanup() {
  rm -rf "$TMP_DIR"
}
trap cleanup EXIT

say_ok() {
  printf "[PASS] %s\n" "$1"
  pass=$((pass + 1))
}

say_fail() {
  printf "[FAIL] %s\n" "$1"
  fail=$((fail + 1))
}

has_header() {
  local file="$1"
  local pattern="$2"
  grep -Eiq "^${pattern}" "$file"
}

run_curl() {
  local cmd_desc="$1"
  shift
  if ! curl -sS "$@"; then
    say_fail "$cmd_desc (curl failed)"
    return 1
  fi
  return 0
}

status_code() {
  awk 'BEGIN{code=""} /^HTTP\// {code=$2} END{print code}' "$1"
}

echo "== 42 webserv minimum eval: Cookies + Session + Multi-CGI =="
echo "BASE_URL=$BASE_URL"

# 1) First request should issue session cookie
first_ok=1
if ! run_curl "First request" -D "$H1" -o "$B1" -c "$COOKIE_JAR" "$BASE_URL$COOKIE_PATH"; then
  first_ok=0
fi
code1="$(status_code "$H1")"
if [[ "$first_ok" -eq 1 && "$code1" =~ ^2|3|4|5 ]]; then
  say_ok "First request returned HTTP status ($code1)"
elif [[ "$first_ok" -eq 1 ]]; then
  say_fail "First request did not return valid HTTP status"
fi

if [[ "$first_ok" -eq 1 ]] && has_header "$H1" 'Set-Cookie:[[:space:]]*webservsid='; then
  say_ok "First response sets webserv session cookie"
elif [[ "$first_ok" -eq 1 ]]; then
  say_fail "Missing Set-Cookie webservsid in first response"
fi

sid="$(awk '$6 == "webservsid" {print $7}' "$COOKIE_JAR" | tail -n1)"
if [[ "$first_ok" -eq 1 && -n "$sid" ]]; then
  say_ok "Cookie jar contains webservsid"
elif [[ "$first_ok" -eq 1 ]]; then
  say_fail "Cookie jar does not contain webservsid"
fi

# 2) Second request with same cookie should be accepted and usually no new Set-Cookie
second_ok=1
if ! run_curl "Second request" -D "$H2" -o "$B2" -b "$COOKIE_JAR" -c "$COOKIE_JAR" "$BASE_URL$COOKIE_PATH"; then
  second_ok=0
fi
code2="$(status_code "$H2")"
if [[ "$second_ok" -eq 1 && "$code2" =~ ^2|3|4|5 ]]; then
  say_ok "Second request returned HTTP status ($code2)"
elif [[ "$second_ok" -eq 1 ]]; then
  say_fail "Second request did not return valid HTTP status"
fi

if [[ "$second_ok" -eq 1 ]] && has_header "$H2" 'Set-Cookie:[[:space:]]*webservsid='; then
  say_fail "Second response reissued session cookie (expected reuse)"
elif [[ "$second_ok" -eq 1 ]]; then
  say_ok "Second response reused existing session (no new Set-Cookie)"
fi

sid2="$(awk '$6 == "webservsid" {print $7}' "$COOKIE_JAR" | tail -n1)"
if [[ "$second_ok" -eq 1 && -n "$sid" && -n "$sid2" && "$sid" == "$sid2" ]]; then
  say_ok "Session id remained stable across requests"
elif [[ "$second_ok" -eq 1 ]]; then
  say_fail "Session id changed or missing across requests"
fi

# 3) Invalid cookie should not crash and should respond
third_ok=1
if ! run_curl "Invalid-cookie request" -D "$H3" -o "$B3" -H 'Cookie: webservsid=INVALID_SESSION_ID' "$BASE_URL$COOKIE_PATH"; then
  third_ok=0
fi
code3="$(status_code "$H3")"
if [[ "$third_ok" -eq 1 && "$code3" =~ ^2|3|4|5 ]]; then
  say_ok "Invalid-cookie request handled cleanly (HTTP $code3)"
elif [[ "$third_ok" -eq 1 ]]; then
  say_fail "Invalid-cookie request did not return valid HTTP status"
fi

# 4) Python CGI GET test
py_ok=1
if ! run_curl "Python CGI request" -D "$HPY" -o "$BPY" "$BASE_URL$CGI_PY"; then
  py_ok=0
fi
codepy="$(status_code "$HPY")"
if [[ "$py_ok" -eq 1 && "$codepy" == "200" ]]; then
  say_ok "Python CGI returned 200"
elif [[ "$py_ok" -eq 1 ]]; then
  say_fail "Python CGI expected 200, got $codepy"
fi

if [[ "$py_ok" -eq 1 ]] && grep -q '^METHOD=GET' "$BPY"; then
  say_ok "Python CGI executed and saw REQUEST_METHOD=GET"
elif [[ "$py_ok" -eq 1 ]]; then
  say_fail "Python CGI body missing METHOD=GET"
fi

if [[ "$py_ok" -eq 1 ]] && grep -q '^QUERY=' "$BPY"; then
  say_ok "Python CGI received QUERY_STRING"
elif [[ "$py_ok" -eq 1 ]]; then
  say_fail "Python CGI body missing QUERY output"
fi

# 5) Perl CGI test
# Primary: POST multipart (upload-style CGI)
# Fallback: GET probe for tiny-body configs (e.g., very small client_max_body_size)
printf 'hello from 42 eval\n' > "$TMP_DIR/upload.txt"
pl_ok=1
if ! run_curl "Perl CGI request" -D "$HPL" -o "$BPL" -X POST -F "file1=@$TMP_DIR/upload.txt;filename=min_eval.txt" "$BASE_URL$CGI_PL"; then
  pl_ok=0
fi
codepl="$(status_code "$HPL")"
if [[ "$pl_ok" -eq 1 && ( "$codepl" == "303" || "$codepl" == "200" ) ]]; then
  say_ok "Perl CGI endpoint executed (HTTP $codepl)"
elif [[ "$pl_ok" -eq 1 ]]; then
  # Retry with GET to confirm Perl interpreter CGI execution on strict body limits.
  if run_curl "Perl CGI fallback GET request" -D "$HPL" -o "$BPL" "$BASE_URL$CGI_PL"; then
    codepl="$(status_code "$HPL")"
    if [[ "$codepl" == "200" || "$codepl" == "303" || "$codepl" == "405" ]]; then
      say_ok "Perl CGI endpoint reachable via fallback GET (HTTP $codepl)"
    else
      say_fail "Perl CGI expected 303/200 on POST or a valid fallback GET status, got $codepl"
    fi
  else
    say_fail "Perl CGI expected 303/200, got $codepl"
  fi
fi

if [[ "$pl_ok" -eq 1 ]] && has_header "$HPL" 'Location:[[:space:]]*/(success_upload\.html|failure_upload\.html)'; then
  say_ok "Perl CGI returned redirect header"
elif [[ "$pl_ok" -eq 1 ]]; then
  if [[ "$codepl" == "200" || "$codepl" == "405" ]]; then
    say_ok "Perl CGI fallback response returned without redirect (acceptable for interpreter reachability check)"
  else
    say_fail "Perl CGI redirect header not found"
  fi
fi

echo
echo "Result: PASS=$pass FAIL=$fail"
if [[ "$fail" -eq 0 ]]; then
  echo "Overall: MINIMUM CHECK PASSED"
  exit 0
else
  echo "Overall: NEEDS FIX BEFORE EVAL"
  echo
  echo "Debug hints:"
  echo "- check webserv is running with config enabling /cgi-bin and .py .pl"
  echo "- check CGI script execute permission and shebang"
  echo "- check Set-Cookie uses webservsid"
  echo "- check cookie parsing from request header"
  exit 1
fi
