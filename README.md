# webserv

> 42 Common Core · Rank 05 · Team project with [Ho Wai Keong](https://github.com/waikeong008) and [Kee Wan Tiew](https://github.com/tiewkeewan)
>
> Personal copy of the team repository `waikeong008/webserv`, with full commit history.

An HTTP/1.1 web server written in **C++98**, inspired by NGINX. It serves static websites, handles uploads and runs CGI scripts, all driven by an NGINX-style configuration file.

## Features
- Non-blocking I/O with a single `select()` event loop handling many clients
- NGINX-style config: multiple servers and ports, `location` blocks, root, index, autoindex, allowed methods, client body size limit, custom error pages, redirections
- `GET`, `POST` and `DELETE` methods, chunked request bodies, file uploads
- CGI execution (Python and Perl scripts)
- Request timeouts and proper HTTP status codes / error pages
- Bonus: cookies and session management

## My part
CGI handling (Python / Perl scripts and forms), file uploads, and request timing / timeout handling in connection management.

## Usage
```bash
make
./webserv webserv.conf
# then open http://localhost:<port> as set in the config
```
