#!/usr/bin/env sh
set -eu

BASE_URL="${BASE_URL:-http://127.0.0.1:8080}"
FAST_CGI_PATH="${FAST_CGI_PATH:-/cgi-bin/test.py}"
SLOW_CGI_PATH="${SLOW_CGI_PATH:-/cgi-bin/slow_exit.py}"

FAST_N="${FAST_N:-100}"
SLOW_N="${SLOW_N:-30}"
PARALLEL="${PARALLEL:-15}"

cleanup() {
	if [ -n "${SERVER_PID:-}" ] && kill -0 "$SERVER_PID" 2>/dev/null; then
		echo "Stopping server (pid=$SERVER_PID)..."
		kill -INT "$SERVER_PID" 2>/dev/null || true
		# give it a moment to exit
		i=0
		while [ "$i" -lt 20 ]; do
			if ! kill -0 "$SERVER_PID" 2>/dev/null; then
				break
			fi
			sleep 0.05
			i=$((i + 1))
		done
		kill -KILL "$SERVER_PID" 2>/dev/null || true
		wait "$SERVER_PID" 2>/dev/null || true
	fi
}
trap cleanup EXIT INT TERM

./scripts/free_ports.sh 8080 8081 >/dev/null

echo "Starting webserv..."
./webserv webserv.conf >/tmp/webserv_stress.log 2>&1 &
SERVER_PID=$!

# Wait until server responds
printf "Waiting for server to respond"
i=0
while [ "$i" -lt 50 ]; do
	if curl -fsS -o /dev/null "$BASE_URL/upload.html" 2>/dev/null; then
		echo " ok"
		break
	fi
	printf "."
	sleep 0.1
	i=$((i + 1))
done

if [ "$i" -ge 50 ]; then
	echo ""
	echo "Server did not respond; tailing log:" 1>&2
	tail -n 80 /tmp/webserv_stress.log 1>&2 || true
	exit 1
fi

echo "Fast CGI load: $FAST_N requests @P=$PARALLEL"
seq 1 "$FAST_N" | xargs -n1 -P "$PARALLEL" sh -c 'curl -fsS -o /dev/null "'$BASE_URL$FAST_CGI_PATH'?i=$1"' sh

echo "Slow CGI load (stdout closes then sleeps): $SLOW_N requests @P=$PARALLEL"
(
	seq 1 "$SLOW_N" | xargs -n1 -P "$PARALLEL" sh -c 'curl -fsS -o /dev/null "'$BASE_URL$SLOW_CGI_PATH'?i=$1"' sh
) &
LOAD_PID=$!

# Probe a static page while slow CGI requests are in-flight.
sleep 0.2

echo "Probing static page during slow CGI load..."
# If the event loop is stalled, this will hit max-time and fail.
curl -fsS --max-time 1 -o /dev/null "$BASE_URL/upload.html"
echo "Static probe: OK"

wait "$LOAD_PID"

echo "All stress requests completed."
