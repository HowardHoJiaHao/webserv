#!/usr/bin/env python3

import sys

def redirect_to_url(url):
    header = f"Location: {url}\n\n"
    sys.stdout.write(header)

if __name__ == "__main__":
    redirect_to_url("https://www.youtube.com/watch?v=a7Lq6ZlSqys")