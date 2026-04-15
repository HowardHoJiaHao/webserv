#!/usr/bin/env python3
import os
import sys
import time

# Write a valid CGI response, then close stdout and keep running.
# This used to trigger a server-wide stall when the server did waitpid(..., 0).

sys.stdout.write("Content-Type: text/plain\r\n\r\n")
sys.stdout.write("OK\n")
sys.stdout.flush()

# Close the stdout pipe to the webserv process.
os.close(1)

# Keep the process alive briefly after stdout is closed.
time.sleep(2)

# curl -i -sS http://127.0.0.1:8080/cgi-bin/slow_exit.py

# seq 1 40 | xargs -n1 -P 15 sh -c 'curl -fsS -o /dev/null "http://127.0.0.1:8080/cgi-bin/slow_exit.py?i=$1"' sh

# curl -N -sS --max-time 1 http://127.0.0.1:8080/cgi-bin/infinite.py
# echo $?

# seq 1 40 | xargs -n1 -P 15 sh -c 'curl -fsS -o /dev/null "http://127.0.0.1:8080/cgi-bin/slow_exit.py?i=$1"' sh
# seq 1 200 | xargs -n1 -P 50 sh -c 'curl -fsS -o /dev/null "http://127.0.0.1:8080/cgi-bin/slow_exit.py?i=$1"' sh
# seq 1 40 = make 40 requests.
# -P 15 = run up to 15 at the same time (parallel).
# curl ... slow_exit.py?i=$1 = hit the endpoint with different query ids.
# -o /dev/null = ignore body output.
# -f = fail on HTTP errors.