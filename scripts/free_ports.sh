#!/usr/bin/env sh
set -eu

if command -v ss >/dev/null 2>&1; then
	:
else
	echo "error: 'ss' not found; install iproute2 or use a different tool." 1>&2
	exit 1
fi

# Usage:
#   ./scripts/free_ports.sh 8080 8081
#   PORTS are positional args; if none are provided, defaults to 8080 8081.

if [ "$#" -gt 0 ]; then
	PORTS="$@"
else
	PORTS="8080 8081"
fi

kill_pid_gracefully() {
	pid="$1"

	if ! kill -0 "$pid" 2>/dev/null; then
		return 0
	fi

	# Try graceful shutdown first
	kill -TERM "$pid" 2>/dev/null || true

	# Wait a short time
	i=0
	while [ "$i" -lt 20 ]; do
		if ! kill -0 "$pid" 2>/dev/null; then
			return 0
		fi
		sleep 0.05
		i=$((i + 1))
	done

	# Force kill
	kill -KILL "$pid" 2>/dev/null || true
}

found_any=0

for port in $PORTS; do
	# ss output includes: users:(("webserv",pid=123,fd=3))
	pids=$(ss -ltnp "sport = :$port" 2>/dev/null | grep -oE 'pid=[0-9]+' | cut -d= -f2 | sort -u || true)

	if [ -z "$pids" ]; then
		echo "port $port: free"
		continue
	fi

	found_any=1
	echo "port $port: killing PIDs: $pids"
	for pid in $pids; do
		# Show command if we can
		cmd=$(ps -p "$pid" -o comm= 2>/dev/null || true)
		if [ -n "$cmd" ]; then
			echo "  - pid $pid ($cmd)"
		else
			echo "  - pid $pid"
		fi
		kill_pid_gracefully "$pid"
	done

done

if [ "$found_any" -eq 0 ]; then
	echo "no listeners found on: $PORTS"
fi
