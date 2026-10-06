# webserv

> 42 Common Core · Rank 05 · Team project with [Ho Wai Keong](https://github.com/waikeong008) and [Kee Wan Tiew](https://github.com/tiewkeewan)
>
> Personal copy of the team repository `waikeong008/webserv`, with full commit history.

## Introduction
webserv is a 42 project where we write our own **HTTP/1.1 web server** in **C++98**, inspired by NGINX. The purpose is to understand what really happens between a browser and a server: how requests and responses are built, how one process can serve many clients at once without blocking, and how a server runs scripts through CGI.

The server is driven by an NGINX-style configuration file that describes the servers, their ports and how each route should behave. It serves static websites, accepts file uploads, deletes files and runs Python / Perl CGI scripts.

## Features
| Feature | Details |
|---|---|
| Event loop | One non-blocking `select()` loop handles every listening socket, client connection and CGI pipe |
| Multiple servers | Several servers on different `host:port` pairs, each with its own root, index and error pages |
| Methods | `GET`, `POST` and `DELETE`, allowed per route. Anything else gets `405 Method Not Allowed` |
| Static files | Index files, and a directory listing when `autoindex` is on |
| Uploads | Files sent with `POST` (multipart forms) are saved in the route's `upload_path`, and can be removed with `DELETE` |
| Request bodies | `Content-Length` and chunked (`Transfer-Encoding: chunked`) bodies. Bodies over `client_max_body_size` get `413 Payload Too Large` |
| Error pages | A custom HTML page for each status code |
| Redirections | `return 301 <url>` |
| CGI | `.py` and `.pl` scripts run with the standard CGI variables (`REQUEST_METHOD`, `QUERY_STRING`, `CONTENT_LENGTH`, `PATH_INFO`, …). A script running over 10 seconds is killed and the client gets `504 Gateway Timeout` |
| Timeouts | A client that goes silent for 5 s while sending headers, takes over 15 s to send all its headers, or stalls for 30 s while sending a body gets `408 Request Timeout` (protects against slow "slowloris" clients) |
| Keep-alive | Connections stay open for more requests unless the client asks to close them |
| Bonus: sessions | Each new visitor gets a `webservsid` session cookie that lasts one hour |

## How it works
```
config file ──► ConfigParser ──► servers + locations
                                        │
clients ◄──► select() loop ──► read request ──► parse + validate ──► route
                  ▲                                                    │
                  │        static file / autoindex / upload / DELETE ◄─┤
                  │        redirect / error page                       │
                  └─── response ◄── CGI (fork + execve, read pipe) ◄───┘
```
1. **Configuration** – The config file is parsed into servers and their `location` blocks. Without a file, a built-in default is used: `127.0.0.1:8080` serving `./www1` and `127.0.0.1:8081` serving `./www2`.
2. **Sockets** – One listening socket is opened for each unique `host:port`.
3. **Event loop** – `select()` watches every socket and CGI pipe, waking up at least once a second to check timeouts. Each connection keeps its own read and write buffers, so no client can block the others.
4. **Parsing** – The request line, headers and body (`Content-Length` or chunked) are parsed and validated. Malformed requests get `400 Bad Request`.
5. **Routing** – The request is matched to the server for that port and to the **longest matching** `location`. The method and body size are checked, then the request is handled as a redirect, CGI script, upload, `DELETE`, static file or directory listing.
6. **CGI** – The server forks, connects pipes for the script's input and output, and runs the script with `execve` and the CGI environment variables. The script's output is read through the same `select()` loop and turned into the HTTP response.
7. **Response** – The status line, headers and body are written back as the socket becomes ready. Error responses use the server's custom error pages.

## Configuration file
Example (from `webserv.conf`):
```nginx
server
{
	listen 127.0.0.1:8080;
	root ./www1;
	index index.html;
	client_max_body_size 1000000;
	error_page 404 /404.html;

	location /
	{
		methods GET POST DELETE;
		autoindex on;
	}

	location /cgi-bin
	{
		allow_methods GET POST;
		cgi_enabled on;
		cgi_ext .py .pl;
	}

	location /Upload
	{
		methods POST DELETE;
		upload_enabled on;
		upload_path ./uploads;
	}
}
```

| Directive | Where | Meaning |
|---|---|---|
| `listen` | server | Address and port to listen on (`127.0.0.1:8080`) |
| `root` | server, location | Folder the files are served from |
| `index` | server, location | File served when a folder is requested |
| `client_max_body_size` | server | Largest request body allowed, in bytes |
| `error_page` | server | Custom page for a status code (`error_page 404 /404.html;`) |
| `location` | server | Settings for every path that starts with this prefix |
| `methods` / `allow_methods` | location | Allowed HTTP methods |
| `autoindex` | location | `on` to list a folder's contents when it has no index file |
| `cgi_enabled`, `cgi_ext` | location | Run files with these extensions as CGI scripts |
| `upload_enabled`, `upload_path` | location | Accept uploads and choose where to save them |
| `return` | location | Redirect (`return 301 https://42kl.edu.my/;`) |

## Clone
Clone the repository:
```bash
git clone https://github.com/HowardHoJiaHao/webserv.git
```

## Compile and Run
To compile, `cd` into the cloned directory and run:
```bash
make
```

This builds the `webserv` executable. Other targets: `make clean` (remove object files), `make fclean` (also remove `webserv`) and `make re` (rebuild from scratch).

To run the program with the sample configuration:
```bash
./webserv webserv.conf
```
- `http://127.0.0.1:8080` – site 1: static pages, directory listing, uploads in `/Upload` and CGI scripts in `/cgi-bin`
- `http://127.0.0.1:8081` – site 2: static pages, and `/42kl` redirects to the 42 Kuala Lumpur website

### Examples
Results from testing with `curl` against the sample configuration:

| Request | Result |
|---|---|
| `curl http://127.0.0.1:8080/` | `200` – the index page |
| `curl http://127.0.0.1:8080/missing.html` | `404` – custom 404 page |
| `curl http://127.0.0.1:8080/cgi-bin/hello.py` | `200` – `Hello CGI GET` |
| `curl -d 'name=howard' http://127.0.0.1:8080/cgi-bin/test.py` | `200` – `METHOD=POST QUERY= BODY=name=howard` |
| `curl -X DELETE http://127.0.0.1:8080/cgi-bin/hello.py` | `405` – `DELETE` is not allowed there |
| `curl -F "file=@notes.txt" http://127.0.0.1:8080/Upload` | `201` – `Upload OK`, file saved in `./uploads` |
| `curl -X DELETE http://127.0.0.1:8080/Upload/<file>` | `204` – file deleted |
| A 1.1 MB body (the limit is 1 MB) | `413` – Payload Too Large |
| A chunked `POST` to `/cgi-bin/test.py` | `200` – the script receives the joined body |
| `curl http://127.0.0.1:8080/cgi-bin/infinite.py` | `504` – the endless script is stopped after 10 s |
| `curl -I http://127.0.0.1:8081/42kl` | `301` – `Location: https://42kl.edu.my/` |

The `scripts/` folder has more tests:

| Script | What it checks |
|---|---|
| `eval_smoke_pretty.sh` | A full pass over the evaluation points (multiple sites, error pages, body size limit, upload, `DELETE`, method checks, CGI and CGI timeout), using `eval_config.conf` |
| `test_error_pages.sh` | Every custom error page |
| `check_bonus_cookie_session.sh`, `min_eval_cookie_cgi.sh` | The session cookie bonus |
| `stress_cgi.sh`, `stress_chunked.py` | Many CGI requests and chunked uploads at once |

## My part
CGI handling (Python / Perl scripts and forms), file uploads, and request timing / timeout handling in connection management.

## Project structure
| Path | Contents |
|---|---|
| `src/main.cpp` | Entry point: load the config, open the sockets, start the event loop |
| `src/config/` | Config file parsing into servers and locations |
| `src/engine/` | The `select()` loop, routing, responses, CGI and session cookies |
| `src/connection/` | Per-client state: buffers, request progress and timers |
| `src/httpHandling/` | HTTP request parsing and validation |
| `src/FileHandler.cpp` | Reading, writing and deleting files |
| `include/` | Headers |
| `webserv.conf`, `eval_config.conf` | Sample and evaluation configurations |
| `www1/`, `www2/` | The two sample websites, error pages and CGI scripts |
| `uploads/` | Where uploaded files are saved |
| `scripts/` | Test scripts |
