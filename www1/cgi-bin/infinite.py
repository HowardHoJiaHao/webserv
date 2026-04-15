#!/usr/bin/env python3
import sys
import time

def main():
	# Send the CGI response header first so the browser knows the content type.
	print("Content-type: text/plain\n")
	sys.stdout.flush()

	# Write an initial message before entering the endless loop.
	print("Starting infinite loop...")
	sys.stdout.flush()

	# Loop forever, printing a timestamp every second.
	while True:
	    # Show that the script is still running and update the current time.
	    print("Looping... " + time.strftime("%H:%M:%S"))
	    # Flush so the browser/server receives each line immediately.
	    sys.stdout.flush()
	    # Sleep for one second between loop messages.
	    time.sleep(1)

	# This return is never reached because the loop never ends.
	return 0

if __name__ == "__main__":
	# Run the CGI entry point when the script starts.
    main()

# timeout 3s python3 www1/cgi-bin/infinite.py