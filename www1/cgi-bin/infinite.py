#!/usr/bin/env python3
import sys
import time

def main():
	print("Content-type: text/plain\n")
	sys.stdout.flush()

	print("Starting infinite loop...")
	sys.stdout.flush()

	while True:
	    print("Looping... " + time.strftime("%H:%M:%S"))
	    sys.stdout.flush()
	    time.sleep(1)
	return 0

if __name__ == "__main__":
    main()
