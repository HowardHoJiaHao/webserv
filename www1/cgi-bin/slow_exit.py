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
