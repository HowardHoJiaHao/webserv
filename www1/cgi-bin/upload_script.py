#!/usr/bin/env python3

import os
import sys
import uuid

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))
UPLOAD_DIR = os.path.join(ROOT_DIR, "archive")
LOG_DIR = os.path.join(ROOT_DIR, "log")
LOG_FILE = os.path.join(LOG_DIR, "upload_script.log")

def log_message(message):
	"""Log messages to a file for debugging"""
	try:
		os.makedirs(LOG_DIR, exist_ok=True)
		with open(LOG_FILE, "a") as log:
			log.write(message + "\n")
	except IOError as e:
		print(f"Error: could not write to log file. {e}", file=sys.stderr)

def error_response(status_code, message):
	"""Generates an HTML error page with the specified status."""
	print(f"Status: {status_code}")
	print("Content-Type: text/html")
	print()  # End of headers
	print("<html><body>")
	print(f"<h1>Error {status_code}</h1>")
	print(f"<p>{message}</p>")
	print("</body></html>")
	sys.exit(1)
def redirect_response(location, status="303 See Other"):
	"""Send a proper CGI redirect response."""
	print(f"Status: {status}")
	print(f"Location: {location}")
	print("Content-Type: text/html")
	print("Content-Length: 0")
	print()


def display_success_page():
	redirect_response("/success_upload.html")

def display_failure_page():
	redirect_response("/failure_upload.html", "303 See Other")


def parse_content_disposition(value):
	params = {}
	parts = [p.strip() for p in value.split(';')]
	if parts:
		params['type'] = parts[0].lower()
	for part in parts[1:]:
		if '=' in part:
			k, v = part.split('=', 1)
			params[k.strip().lower()] = v.strip().strip('"')
	return params


def extract_uploaded_file(field_name, content_type, body):
	"""Minimal multipart parser for one file field."""
	if 'boundary=' not in content_type:
		return None, None
	boundary = content_type.split('boundary=', 1)[1].strip().strip('"')
	if not boundary:
		return None, None

	marker = ('--' + boundary).encode('utf-8')
	parts = body.split(marker)
	for part in parts:
		part = part.strip()
		if not part or part == b'--':
			continue
		if b'\r\n\r\n' not in part:
			continue
		headers_raw, content = part.split(b'\r\n\r\n', 1)
		headers = {}
		for line in headers_raw.split(b'\r\n'):
			if b':' not in line:
				continue
			k, v = line.split(b':', 1)
			headers[k.decode('utf-8', errors='ignore').strip().lower()] = v.decode('utf-8', errors='ignore').strip()

		disp = headers.get('content-disposition', '')
		params = parse_content_disposition(disp)
		if params.get('type') != 'form-data':
			continue
		if params.get('name') != field_name:
			continue
		filename = params.get('filename', '')
		if not filename:
			continue

		if content.endswith(b'\r\n'):
			content = content[:-2]
		if content.endswith(b'--'):
			content = content[:-2]
		return filename, content

	return None, None


def main():
	"""Main function to handle the upload request"""
	log_message("Starting CGI script")

	# Ensure upload directory exists
	try:
		os.makedirs(UPLOAD_DIR, exist_ok=True)
	except OSError as e:
		log_message(f"Failed to create upload directory: {e}")
		error_response("500 Internal Server Error", "Server configuration error")

	# Check request method
	request_method = os.environ.get('REQUEST_METHOD', '')
	if request_method != 'POST':
		log_message(f"Invalid request method: {request_method}")
		error_response("405 Method Not Allowed", "Only POST requests are allowed")

	# Check content type
	content_type = os.environ.get('CONTENT_TYPE', '')
	if not content_type.startswith('multipart/form-data'):
		log_message(f"Invalid content type: {content_type}")
		error_response("400 Bad Request", "Expected multipart/form-data")

	try:
		# Read raw request body
		content_length = int(os.environ.get('CONTENT_LENGTH', '0') or '0')
		if content_length <= 0:
			log_message("No request body")
			error_response("400 Bad Request", "Empty request body")

		raw_body = sys.stdin.buffer.read(content_length)
		file_name_from_form, file_bytes = extract_uploaded_file('file1', content_type, raw_body)
		if not file_name_from_form:
			log_message("No file1 field in multipart payload")
			error_response("400 Bad Request", "No file field found")

		# Sanitize filename for security
		original_filename = os.path.basename(file_name_from_form)
		if not original_filename:
			log_message("Empty filename after sanitization")
			error_response("400 Bad Request", "Invalid filename")
		safe_original = original_filename.replace("/", "_").replace("\\", "_")

		# Generate unique filename to prevent overwrites
		unique_id = uuid.uuid4().hex[:8]
		filename, file_extension = os.path.splitext(safe_original)
		safe_filename = f"{filename}_{unique_id}{file_extension}"
		save_path = os.path.join(UPLOAD_DIR, safe_filename)

		log_message(f"Original filename: {original_filename}")
		log_message(f"Safe filename: {safe_filename}")
		log_message(f"Save path: {save_path}")

		# Save the file
		try:
			with open(save_path, 'wb') as f:
				f.write(file_bytes)

			file_size = os.path.getsize(save_path)
			log_message(f"File saved successfully. Size: {file_size} bytes")

			# # Return success response
			display_success_page()

		except IOError as e:
			log_message(f"Error saving file: {e}")
			# error_response("500 Internal Server Error", "Failed to save file")
			display_failure_page()

	except Exception as e:
		log_message(f"Unexpected error: {str(e)}")
		# error_response("500 Internal Server Error", "An unexpected error occurred")
		display_failure_page()

# Run the main function
if __name__ == "__main__":
	main()
