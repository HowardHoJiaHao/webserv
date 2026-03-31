#!/usr/bin/env python3

import sys

def redirect_to_url(url):
    header = f"Location: {url}\n\n"
    sys.stdout.write(header)

if __name__ == "__main__":
    redirect_to_url("http://localhost:8080/upload.html")
    # redirect_to_url("https://www.google.com/maps/place/42+Kuala+Lumpur/@3.0663331,101.6036901,2209m/data=!3m1!1e3!4m6!3m5!1s0x31cc4d794d5ae8df:0xef22ed33ec0f131e!8m2!3d3.0664286!4d101.6062687!16s%2Fg%2F11pcsd6crg?entry=ttu&g_ep=EgoyMDI1MDkwMy4wIKXMDSoASAFQAw%3D%3D")