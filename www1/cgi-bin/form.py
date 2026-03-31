#!/usr/bin/env python3

import datetime
import html
import os
import sys
from urllib.parse import parse_qs


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))
DATA_DIR = os.path.join(ROOT_DIR, "archive")
LOG_DIR = os.path.join(ROOT_DIR, "log")
LOG_FILE = os.path.join(LOG_DIR, "upload_script.log")


def log_message(message):
    try:
        os.makedirs(LOG_DIR, exist_ok=True)
        timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        with open(LOG_FILE, "a", encoding="utf-8") as f:
            f.write(f"[{timestamp}] [form.py] {message}\n")
    except Exception:
        pass


def send_response(status, body):
    body_bytes = body.encode("utf-8")
    sys.stdout.write(f"Status: {status}\r\n")
    sys.stdout.write("Content-Type: text/html; charset=UTF-8\r\n")
    sys.stdout.write(f"Content-Length: {len(body_bytes)}\r\n")
    sys.stdout.write("\r\n")
    sys.stdout.flush()
    sys.stdout.buffer.write(body_bytes)


def parse_form_body():
    method = os.environ.get("REQUEST_METHOD", "").upper()
    if method != "POST":
        log_message(f"Invalid request method: {method}")
        return None, "Only POST requests are allowed"

    content_type = os.environ.get("CONTENT_TYPE", "")
    if "application/x-www-form-urlencoded" not in content_type:
        log_message(f"Invalid content type: {content_type}")
        return None, "Expected application/x-www-form-urlencoded"

    try:
        content_length = int(os.environ.get("CONTENT_LENGTH", "0") or "0")
    except ValueError:
        content_length = 0

    raw = sys.stdin.read(content_length) if content_length > 0 else ""
    parsed = parse_qs(raw)

    def get_value(key, default=""):
        return parsed.get(key, [default])[0].strip()

    data = {
        "name": get_value("name", "Unnamed Adventurer"),
        "age": get_value("age", "Unknown Level"),
        "gender": get_value("gender", "Undisclosed"),
        "class": get_value("class", "Wanderer"),
        "biodata": get_value("biodata", "No notes provided"),
    }
    return data, None


def save_entry(data):
    os.makedirs(DATA_DIR, exist_ok=True)
    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    filename = os.path.join(DATA_DIR, f"quest_log_entry_{timestamp}.txt")
    with open(filename, "w", encoding="utf-8") as f:
        f.write("--- Quest Log Entry ---\n\n")
        f.write(f"Adventurer Name: {data['name']}\n")
        f.write(f"Current Level: {data['age']}\n")
        f.write(f"Gender: {data['gender']}\n")
        f.write(f"Character Class: {data['class']}\n")
        f.write(f"Quest Notes:\n{data['biodata']}\n")
    log_message(f"Saved quest log entry to {filename}")
    return filename


def render_result_page(ok, data=None, filename="", error_message=""):
    if ok:
        name = html.escape(data["name"])
        age = html.escape(data["age"])
        gender = html.escape(data["gender"])
        role = html.escape(data["class"])
        notes = html.escape(data["biodata"])
        saved_name = html.escape(os.path.basename(filename))
        title = "Form submitted successfully"
        status_line = f"<p class=\"muted\"><b>{name}</b>, your entry has been saved to <i>{saved_name}</i>.</p>"
        details = f"""
<div class=\"table-wrapper\" style=\"margin-top:1rem;\">
    <table class=\"archive-table\">
        <tbody>
            <tr><th style=\"width:220px;\">Name</th><td>{name}</td></tr>
            <tr><th>Age</th><td>{age}</td></tr>
            <tr><th>Gender</th><td>{gender}</td></tr>
            <tr><th>Class</th><td>{role}</td></tr>
            <tr><th>Bio</th><td style=\"white-space:pre-wrap; word-break:break-word;\">{notes}</td></tr>
        </tbody>
    </table>
</div>
"""
    else:
        title = "Form submission failed"
        status_line = "<p class=\"muted\"><b>Oops!</b> There was an error saving your entry.</p>"
        details = f"<p class=\"muted\">Error: {html.escape(error_message)}</p>"

    return f"""<!DOCTYPE html>
<html lang=\"en\">
<head>
  <meta charset=\"UTF-8\">
    <title>{title}</title>
    <link rel=\"stylesheet\" href=\"/style.css\">
</head>
<body class=\"body-top\">
    <div id=\"header-placeholder\"></div>
    <script src=\"/header.js\"></script>
    <div class=\"hero-wrap\">
        <div class=\"container form-card\">
            <h2 class=\"form-title\">{title}</h2>
            {status_line}
            {details}
        </div>
    </div>
</body>
</html>"""


def main():
    log_message("Request started")
    data, parse_error = parse_form_body()
    if parse_error:
        body = render_result_page(False, error_message=parse_error)
        send_response("400 Bad Request", body)
        log_message(f"Request failed with parse error: {parse_error}")
        return

    try:
        filename = save_entry(data)
        body = render_result_page(True, data=data, filename=filename)
        send_response("200 OK", body)
        log_message("Request completed successfully")
    except Exception as e:
        body = render_result_page(False, error_message=str(e))
        send_response("500 Internal Server Error", body)
        log_message(f"Unhandled error: {e}")


if __name__ == "__main__":
    main()
