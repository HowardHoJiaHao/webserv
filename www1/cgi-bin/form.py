#!/usr/bin/env python3
# Tells the system to execute this script using Python 3

# Import modules: datetime (time), html (escaping), os (file paths), sys (stdin/stdout), parse_qs (URL parsing)
import datetime
import html
import os
import sys
from urllib.parse import parse_qs

# Define directory paths for storing data
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))  # Current script directory
ROOT_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))  # Parent directory (www1/)
DATA_DIR = os.path.join(ROOT_DIR, "archive")  # Directory to save form submissions
LOG_DIR = os.path.join(ROOT_DIR, "log")  # Directory for log files
LOG_FILE = os.path.join(LOG_DIR, "upload_script.log")  # Full log file path


def log_message(message):
    # Logs messages to a file with timestamps (useful for debugging CGI scripts)
    try:
        os.makedirs(LOG_DIR, exist_ok=True)  # Create log directory if it doesn't exist
        timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")  # Current time
        with open(LOG_FILE, "a", encoding="utf-8") as f:  # 'a' = append mode
            f.write(f"[{timestamp}] [form.py] {message}\n")
    except Exception:
        pass  # Silently ignore logging errors - don't crash the script


def send_response(status, body):
    # Sends HTTP response headers and body to the client (webserver will send to browser)
    body_bytes = body.encode("utf-8")  # Convert HTML string to bytes
    sys.stdout.write(f"Status: {status}\r\n")  # HTTP status (e.g., "200 OK")
    sys.stdout.write("Content-Type: text/html; charset=UTF-8\r\n")  # Tell browser it's HTML
    sys.stdout.write(f"Content-Length: {len(body_bytes)}\r\n")  # Size of the response
    sys.stdout.write("\r\n")  # Blank line separates headers from body
    sys.stdout.flush()  # Ensure headers are sent immediately
    sys.stdout.buffer.write(body_bytes)  # Write the actual HTML body


def parse_form_body():
    # Parses the form data sent by the browser via POST request
    method = os.environ.get("REQUEST_METHOD", "").upper()  # Get HTTP method (GET/POST/etc)
    if method != "POST":
        log_message(f"Invalid request method: {method}")
        return None, "Only POST requests are allowed"  # Only accept POST

    content_type = os.environ.get("CONTENT_TYPE", "")  # Check data format
    if "application/x-www-form-urlencoded" not in content_type:
        log_message(f"Invalid content type: {content_type}")
        return None, "Expected application/x-www-form-urlencoded"  # Form data must be URL-encoded

    try:
        content_length = int(os.environ.get("CONTENT_LENGTH", "0") or "0")  # Size of form data
    except ValueError:
        content_length = 0

    # Read the form data from stdin (webserver provides it here)
    raw = sys.stdin.read(content_length) if content_length > 0 else ""
    parsed = parse_qs(raw)  # Convert URL-encoded string to dictionary

    def get_value(key, default=""):
        # Helper function: safely extract form field values
        return parsed.get(key, [default])[0].strip()  # Get first value, remove whitespace

    # Extract all form fields and use defaults if missing
    data = {
        "name": get_value("name", "Unnamed Adventurer"),
        "age": get_value("age", "Unknown Level"),
        "gender": get_value("gender", "Undisclosed"),
        "class": get_value("class", "Wanderer"),
        "biodata": get_value("biodata", "No notes provided"),
    }
    return data, None  # Return parsed data and no error


def save_entry(data):
    # Saves the form submission to a file in the archive directory
    os.makedirs(DATA_DIR, exist_ok=True)  # Ensure archive directory exists
    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")  # Unique timestamp
    filename = os.path.join(DATA_DIR, f"quest_log_entry_{timestamp}.txt")  # Create filename
    with open(filename, "w", encoding="utf-8") as f:  # Create new file
        f.write("--- Quest Log Entry ---\n\n")
        f.write(f"Adventurer Name: {data['name']}\n")
        f.write(f"Current Level: {data['age']}\n")
        f.write(f"Gender: {data['gender']}\n")
        f.write(f"Character Class: {data['class']}\n")
        f.write(f"Quest Notes:\n{data['biodata']}\n")
    log_message(f"Saved quest log entry to {filename}")
    return filename  # Return filename for confirmation


def render_result_page(ok, data=None, filename="", error_message=""):
    # Generates HTML response page (success or error)
    if ok:
        # SUCCESS case: display the submitted form data
        name = html.escape(data["name"])  # Escape HTML special chars (security)
        age = html.escape(data["age"])
        gender = html.escape(data["gender"])
        role = html.escape(data["class"])
        notes = html.escape(data["biodata"])
        saved_name = html.escape(os.path.basename(filename))
        title = "Form submitted successfully"
        status_line = f"<p class=\"muted\"><b>{name}</b>, your entry has been saved to <i>{saved_name}</i>.</p>"
        # Create HTML table showing the saved data
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
        # ERROR case: display error message
        title = "Form submission failed"
        status_line = "<p class=\"muted\"><b>Oops!</b> There was an error saving your entry.</p>"
        details = f"<p class=\"muted\">Error: {html.escape(error_message)}</p>"
    # Return complete HTML page (same for both success and error, just different content)
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
    # Main function: orchestrates the entire form submission process
    log_message("Request started")
    
    # Step 1: Parse the form data from POST request
    data, parse_error = parse_form_body()
    if parse_error:
        # If parsing failed, send error response
        body = render_result_page(False, error_message=parse_error)
        send_response("400 Bad Request", body)
        log_message(f"Request failed with parse error: {parse_error}")
        return

    try:
        # Step 2: Save form data to file
        filename = save_entry(data)
        
        # Step 3: Send success response with the saved data
        body = render_result_page(True, data=data, filename=filename)
        send_response("200 OK", body)
        log_message("Request completed successfully")
    except Exception as e:
        # If saving failed, send error response
        body = render_result_page(False, error_message=str(e))
        send_response("500 Internal Server Error", body)
        log_message(f"Unhandled error: {e}")


# Entry point: only run main() if script is executed directly (not imported)
if __name__ == "__main__":
    main()
