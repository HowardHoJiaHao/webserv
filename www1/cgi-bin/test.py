#!/usr/bin/env python3
import os
import sys

body = sys.stdin.buffer.read()

sys.stdout.write("Content-Type: text/plain\r\n\r\n")
sys.stdout.write("METHOD=" + os.environ.get("REQUEST_METHOD", "") + "\n")
sys.stdout.write("QUERY=" + os.environ.get("QUERY_STRING", "") + "\n")
sys.stdout.write("BODY=" + body.decode("utf-8", errors="replace") + "\n")
