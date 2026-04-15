#!/usr/bin/env python3

import json
import os
import sys
import datetime
from urllib.parse import parse_qs


# Resolve project paths relative to this CGI script location.
ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
ARCHIVE_DIR = os.path.join(ROOT_DIR, "archive")
LOG_DIR = os.path.join(ROOT_DIR, "log")
LOG_FILE = os.path.join(LOG_DIR, "upload_script.log")


def log_message(message):
    # Best-effort logging: failures here should not stop request handling.
    try:
        os.makedirs(LOG_DIR, exist_ok=True)
        timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        with open(LOG_FILE, "a", encoding="utf-8") as f:
            f.write(f"[{timestamp}] [delete.py] {message}\n")
    except Exception:
        pass


def safe_join(base, *paths):
    """Join paths and reject anything that escapes the base directory."""
    full = os.path.abspath(os.path.join(base, *paths))
    if not full.startswith(os.path.abspath(base) + os.sep):
        raise ValueError("Invalid path")
    return full


def list_files():
    # Return only regular files from the archive directory, sorted case-insensitively.
    if not os.path.isdir(ARCHIVE_DIR):
        log_message("Archive directory missing; returning empty list")
        return []
    files = []
    for name in sorted(os.listdir(ARCHIVE_DIR), key=str.lower):
        full = os.path.join(ARCHIVE_DIR, name)
        if os.path.isfile(full):
            files.append(name)
    return files


def delete_file(name):
    # Reject empty names, traversal attempts, and direct path separators.
    if not name or ".." in name or "/" in name or "\\" in name:
        log_message(f"Rejected invalid filename: {name}")
        return False, "Invalid filename"
    try:
        # Build the full path safely inside the archive directory.
        full = safe_join(ARCHIVE_DIR, name)
        if not os.path.exists(full):
            log_message(f"Delete target not found: {name}")
            return False, "Not found"
        # Remove the file once it has passed validation.
        os.remove(full)
        log_message(f"Deleted file: {name}")
        return True, "Deleted"
    except Exception as e:
        log_message(f"Delete error for {name}: {e}")
        return False, str(e)


def respond(status_code, status_message, content_type, body):
    # Emit a complete CGI response with headers and body bytes.
    if isinstance(body, str):
        body_bytes = body.encode("utf-8")
    else:
        body_bytes = body
    sys.stdout.write(
        f"Status: {status_code} {status_message}\r\n"
        f"Content-Type: {content_type}\r\n"
        f"Content-Length: {len(body_bytes)}\r\n"
        "\r\n"
    )
    sys.stdout.flush()
    sys.stdout.buffer.write(body_bytes)
    sys.stdout.flush()


def send_json(status_code, status_message, payload):
    # Convenience wrapper for JSON responses.
    respond(status_code, status_message, "application/json", json.dumps(payload))


def render_page(files):
    # Render the archive management page and embed the initial file list.
    files_json = json.dumps(files)
    return f"""<!DOCTYPE html>
<html lang=\"en\">
<head>
    <meta charset=\"UTF-8\" />
    <link rel=\"stylesheet\" href=\"/style.css\">
    <title>Archive</title>
</head>
<body class=\"body-top\">
    <div id=\"header-placeholder\"></div>
    <script src=\"/header.js\"></script>
    <div class=\"hero-wrap\">
        <div class=\"container form-card\">
            <h2 class=\"form-title\">Archive (Python CGI)</h2>
            <p class=\"muted\">Uploaded files stored on the server. Fetches live data from the archive endpoint.</p>
            <div class=\"table-wrapper\">
                <table class=\"archive-table\" id=\"archive-table\">
                    <thead>
                        <tr><th>Name</th><th style=\"width: 140px;\">Actions</th></tr>
                    </thead>
                    <tbody id=\"archive-body\">
                        <tr><td colspan=\"2\" class=\"muted\">Loading...</td></tr>
                    </tbody>
                </table>
            </div>
            <div class=\"actions\" style=\"margin-top:1rem; display:flex; gap:0.5rem;\">
                <button class=\"btn-primary\" id=\"refresh-btn\" type=\"button\">Refresh</button>
                <span id=\"status\" class=\"muted\"></span>
            </div>
        </div>
    </div>

    <script>
    // Use the same CGI endpoint for both listing and deleting files.
    const API_LIST = window.location.pathname + '?format=json';
    const API_DELETE = window.location.pathname;
    const initialFiles = {files_json};

    async function fetchList() {{
        // Update the table with the latest archive contents from the server.
        const statusEl = document.getElementById('status');
        const bodyEl = document.getElementById('archive-body');
        statusEl.textContent = '';
        bodyEl.innerHTML = '<tr><td colspan=\"2\" class=\"muted\">Loading...</td></tr>';
        try {{
            // Request the JSON list variant of this same endpoint.
            const res = await fetch(API_LIST, {{ headers: {{ 'Accept': 'application/json' }} }});
            if (!res.ok) throw new Error('Failed to load list');
            const data = await res.json();
            renderRows(data.files || []);
        }} catch (err) {{
            bodyEl.innerHTML = '<tr><td colspan=\"2\" class=\"muted\">Error loading files.</td></tr>';
            statusEl.textContent = err.message;
        }}
    }}

            // Draw each filename into the table and add a Delete button beside it.
    function renderRows(files) {{
        const bodyEl = document.getElementById('archive-body');
        const statusEl = document.getElementById('status');
        if (!files.length) {{
            bodyEl.innerHTML = '<tr><td colspan=\"2\" class=\"muted\">No files found.</td></tr>';
            return;
        }}
        bodyEl.innerHTML = '';
        files.forEach(name => {{
            const tr = document.createElement('tr');
            const nameTd = document.createElement('td');
            nameTd.textContent = name;
            const actionTd = document.createElement('td');
            const btn = document.createElement('button');
            btn.textContent = 'Delete';
            btn.className = 'btn-primary';
            btn.onclick = async () => {{
                // Send the selected filename back to the CGI delete handler.
                statusEl.textContent = 'Deleting ' + name + '...';
                const resp = await fetch(API_DELETE, {{
                    method: 'POST',
                    headers: {{ 'Content-Type': 'application/json' }},
                    body: JSON.stringify({{ name }})
                }});
                if (resp.ok) {{
                    // Refresh the list so the removed file disappears immediately.
                    statusEl.textContent = 'Deleted ' + name;
                    fetchList();
                }} else {{
                    // Keep the row and show the HTTP error status if deletion failed.
                    statusEl.textContent = 'Delete failed (' + resp.status + ')';
                }}
            }};
            actionTd.appendChild(btn);
            tr.appendChild(nameTd);
            tr.appendChild(actionTd);
            bodyEl.appendChild(tr);
        }});
    }}

    // Reload the archive listing when the Refresh button is clicked.
    document.getElementById('refresh-btn').addEventListener('click', fetchList);
    // Populate the table once when the page loads.
    renderRows(initialFiles);
    fetchList();
    </script>
</body>
</html>
"""


def parse_body():
    # Read the request body length from the CGI environment.
    length = 0
    try:
        length = int(os.environ.get("CONTENT_LENGTH", "0") or 0)
    except ValueError:
        length = 0
    # Read exactly CONTENT_LENGTH bytes if the client sent a body.
    body = sys.stdin.read(length) if length > 0 else ""
    ct = os.environ.get("CONTENT_TYPE", "")
    name = ""
    # Accept JSON, form-encoded, or raw-text delete requests.
    if "application/json" in ct:
        try:
            name = json.loads(body).get("name", "")
        except Exception:
            name = ""
    elif "application/x-www-form-urlencoded" in ct:
        parsed = parse_qs(body)
        name = parsed.get("name", [""])[0]
    else:
        name = body.strip()
    return name


def main():
    # Dispatch by HTTP method and query string.
    method = os.environ.get("REQUEST_METHOD", "GET").upper()
    query = parse_qs(os.environ.get("QUERY_STRING", ""))
    log_message(f"Request started method={method} query={os.environ.get('QUERY_STRING', '')}")

    # JSON list endpoint
    if method == "GET" and query.get("format", [""])[0].lower() == "json":
        # Return the current archive file list as JSON.
        files = list_files()
        log_message(f"Returned archive list count={len(files)}")
        return send_json(200, "OK", {"files": files})

    # Delete handler
    if method in ("POST", "DELETE"):
        # Parse the requested filename from the request body.
        name = parse_body()
        ok, msg = delete_file(name)
        # Map the delete result to an HTTP status code.
        status = 200 if ok else (404 if msg == "Not found" else 400)
        log_message(f"Delete request name={name} result={ok} status={status}")
        return send_json(status, "OK" if ok else "Error", {"success": ok, "message": msg})

    # Default: render HTML page
    # Serve the archive UI when no JSON or delete action is requested.
    files = list_files()
    page = render_page(files)
    log_message("Rendered archive page")
    respond(200, "OK", "text/html", page)


if __name__ == "__main__":
    # Run the CGI entry point.
    main()
