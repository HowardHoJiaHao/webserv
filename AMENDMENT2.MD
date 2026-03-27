# Webserv — Full Fix Guide

---

## Table of Contents

1. [Critical: Double-advance iterator in `processOutgoingData`](#1-critical-double-advance-iterator-in-processoutgoingdata)
2. [Critical: `build400Response()` declared but not defined](#2-critical-build400response-declared-but-not-defined)
3. [Bug: `routeRequest` hardcodes `shouldClose = false`](#3-bug-routerequest-hardcodes-shouldclose--false)
4. [Bug: Duplicate `Content-Length` not rejected in `extractRequest`](#4-bug-duplicate-content-length-not-rejected-in-extractrequest)
5. [Bug: Chunked chunk-extensions not stripped](#5-bug-chunked-chunk-extensions-not-stripped)
6. [Bug: Dead members `_raw` and `_headersParsed`](#6-bug-dead-members-_raw-and-_headersParsed)
7. [Design gap: Config not connected to routing](#7-design-gap-config-not-connected-to-routing)
8. [Design gap: Allowed methods not enforced](#8-design-gap-allowed-methods-not-enforced)
9. [Design gap: Max body size not from config](#9-design-gap-max-body-size-not-from-config)
10. [Design gap: Timeout does not apply to WRITING connections](#10-design-gap-timeout-does-not-apply-to-writing-connections)
11. [Missing: DELETE method](#11-missing-delete-method)
12. [Missing: Autoindex (directory listing)](#12-missing-autoindex-directory-listing)
13. [Missing: Custom error pages from config](#13-missing-custom-error-pages-from-config)
14. [Minor: Console spam and dead code cleanup](#14-minor-console-spam-and-dead-code-cleanup)

---

## 1. Critical: Double-advance iterator in `processOutgoingData`

### The problem

In `engine.cpp`, `processOutgoingData` has `++it` at the bottom of the loop body. But
several branches inside the loop also call `++it` or `erase(it++)`. When those branches
execute, the bottom `++it` fires again, skipping the next connection entirely.

**Bad pattern:**

```cpp
for (auto it = _clientConnections.begin(); it != _clientConnections.end();)
{
    if (FD_ISSET(clientFd, &writeSet))
    {
        if (sentByte > 0) { /* ... */ }
        else if (sentByte == 0)
        {
            delete it->second;
            _clientConnections.erase(it++);  // <-- already advances
            continue;
        }
        // ... more branches with ++it or erase(it++) ...
    }
    ++it;  // <-- fires even after branches that already advanced
}
```

### The fix

Every branch must either `continue` (to skip the bottom `++it`) or fall through to it.
The cleanest approach is to never call `++it` inside a branch — only use `erase(it++)`,
and always `continue` after a deletion. Let the bottom `++it` handle the non-deletion
case.

```cpp
void Engine::processOutgoingData(fd_set& writeSet)
{
    for (std::map<int, Connection*>::iterator it = _clientConnections.begin();
         it != _clientConnections.end();)
    {
        int clientFd = it->first;
        Connection* conn = it->second;

        if (!FD_ISSET(clientFd, &writeSet))
        {
            ++it;
            continue;
        }

        std::string& writebuffer = conn->getWriteBuffer();
        if (writebuffer.empty())
        {
            ++it;
            continue;
        }

        ssize_t sentByte = send(clientFd, writebuffer.data(), writebuffer.size(), 0);

        if (sentByte > 0)
        {
            writebuffer.erase(0, sentByte);
            conn->updateActivity();

            if (writebuffer.empty())
            {
                if (conn->shouldClose())
                {
                    delete conn;
                    _clientConnections.erase(it++);
                    continue;
                }
                // Reset for next request (keep-alive)
                conn->setRequestState(Connection::READING_HEADERS);
                conn->setState(Connection::READING);
            }
            ++it;
        }
        else if (sentByte == 0)
        {
            delete conn;
            _clientConnections.erase(it++);
        }
        else // sentByte < 0
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                ++it;
                continue;
            }
            perror("send");
            delete conn;
            _clientConnections.erase(it++);
        }
    }
}
```

**Rule to follow:** After every `erase(it++)`, always `continue`. Only one code path
should reach `++it` per iteration — the normal non-deletion path at the end.

---

## 2. Critical: `build400Response()` declared but not defined

### The problem

`engine.hpp` declares:

```cpp
std::string build400Response();
```

But `engine.cpp` never defines it. The linker will fail if this symbol is ever resolved.
You currently call `buildResponse("400 Bad request", ...)` directly, which works, but
the dangling declaration is a time-bomb.

### The fix

**Option A — Delete the declaration from `engine.hpp`:**

```cpp
// engine.hpp — remove this line:
// std::string build400Response();
```

**Option B — Add the definition to `engine.cpp` for consistency with 404/405:**

```cpp
std::string Engine::build400Response()
{
    return buildResponse("400 Bad Request", "Bad Request", "text/plain", true);
}
```

Then replace the inline `buildResponse("400 Bad request", ...)` calls with
`build400Response()`. Note the corrected capitalisation: `"Bad Request"` not
`"Bad request"`.

---

## 3. Bug: `routeRequest` hardcodes `shouldClose = false`

### The problem

Every `buildResponse` call inside `routeRequest` and `handlePost` passes `false` for
`shouldClose`:

```cpp
return buildResponse("200 OK", content, mime, false);  // always keep-alive
```

This means the `Connection: keep-alive` header is written into the response regardless
of what the client requested. The fix in `handleClientRequest` that does a string
replacement after the fact is fragile and hard to reason about.

### The fix

Pass `shouldClose` into `routeRequest` and `handlePost`:

```cpp
// engine.hpp
std::string routeRequest(const HttpRequest& request, bool shouldClose);
std::string handlePost(const HttpRequest& request, bool shouldClose);
```

```cpp
// engine.cpp — in handleClientRequest
bool shouldClose = request.shouldCloseConnection();
if (shouldClose)
    conn->setShouldClose(true);

std::string response = routeRequest(request, shouldClose);
conn->getWriteBuffer() += response;
```

```cpp
std::string Engine::routeRequest(const HttpRequest& request, bool shouldClose)
{
    if (request.getMethod() == "GET")
    {
        if (request.getPath().find("..") != std::string::npos)
            return buildResponse("403 Forbidden", "Forbidden", "text/plain", shouldClose);

        std::string path = FileHandler::resolvePath(request.getPath());
        struct stat s;
        if (stat(path.c_str(), &s) == 0 && S_ISDIR(s.st_mode))
            path += "/index.html";

        if (!FileHandler::fileExists(path))
            return buildResponse("404 Not Found", "Not Found", "text/plain", shouldClose);

        std::string content = FileHandler::readFile(path);
        std::string mime = FileHandler::getMimeType(path);
        return buildResponse("200 OK", content, mime, shouldClose);
    }
    if (request.getMethod() == "POST")
        return handlePost(request, shouldClose);

    return buildResponse("405 Method Not Allowed", "Method Not Allowed", "text/plain", shouldClose);
}

std::string Engine::handlePost(const HttpRequest& request, bool shouldClose)
{
    const std::string& body = request.getBody();
    std::string path = "./www/upload.txt";

    int fd = open(path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0)
        return buildResponse("500 Internal Server Error", "Open Failed", "text/plain", shouldClose);

    size_t total = 0;
    while (total < body.size())
    {
        ssize_t written = write(fd, body.data() + total, body.size() - total);
        if (written <= 0)
        {
            close(fd);
            return buildResponse("500 Internal Server Error", "Write failed", "text/plain", shouldClose);
        }
        total += written;
    }
    close(fd);
    return buildResponse("200 OK", "Upload OK", "text/plain", shouldClose);
}
```

Remove the string-replacement hack from `handleClientRequest`:

```cpp
// DELETE these lines:
// if (shouldClose)
// {
//     size_t pos = response.find("Connection: keep-alive");
//     if (pos != std::string::npos)
//         response.replace(pos, 24, "Connection: close");
// }
```

---

## 4. Bug: Duplicate `Content-Length` not rejected in `extractRequest`

### The problem

In `extractRequest.cpp`, the header parsing loop overwrites `contentLength` silently
when it sees a second `content-length` header:

```cpp
if (key == "content-length")
{
    // ... parses and overwrites contentLength without checking if already set
    contentLength = static_cast<size_t>(parsed);
    hasContentLength = true;
}
```

Per RFC 7230 §3.3.2, a request with more than one `Content-Length` header field must be
rejected with 400.

### The fix

Track whether you have already seen `content-length` and reject on a second occurrence:

```cpp
bool hasContentLength = false;
size_t contentLength = 0;
bool duplicateCL = false;

// inside the header parsing loop:
if (key == "content-length")
{
    if (hasContentLength)
    {
        duplicateCL = true;
        break;
    }
    // ... parse value ...
    contentLength = static_cast<size_t>(parsed);
    hasContentLength = true;
}

// after the loop:
if (duplicateCL)
    return false;  // caller will treat as incomplete — or add a dedicated error path
```

> **Note:** Returning `false` here causes `handleClientRequest` to sit and wait for
> more data, which is not quite right for a hard protocol error. A cleaner approach is
> to use an out-parameter or exception to signal "reject with 400" versus "not enough
> data yet". For now, returning `false` on duplicate CL is safe because
> `httpRequest::parse()` will also throw "Duplicate header" on the same request,
> producing the 400. The `extractRequest` guard just prevents the body extraction from
> being fooled by the inflated second value.

---

## 5. Bug: Chunked chunk-extensions not stripped

### The problem

RFC 7230 §4.1 allows chunk size lines to have extensions:

```
5; name=value\r\n
Hello\r\n
```

Your current check:

```cpp
if (endptr == hexSize.c_str() || *endptr != '\0')
    return false;
```

`strtoul` stops at `;` and leaves `endptr` pointing there. `*endptr != '\0'` is true,
so the function incorrectly returns `false` — treating a valid chunked request as
incomplete.

### The fix

Strip everything from `;` onwards before validation:

```cpp
static std::string stripChunkExtensions(const std::string& line)
{
    size_t semicolon = line.find(';');
    if (semicolon != std::string::npos)
        return line.substr(0, semicolon);
    return line;
}

// inside parseChunkedBody, replace:
//   std::string hexSize = buffer.substr(pos, lineEnd - pos);
// with:
std::string rawLine = buffer.substr(pos, lineEnd - pos);
std::string hexSize = stripChunkExtensions(rawLine);

// trim trailing whitespace from hexSize as well
while (!hexSize.empty() && (hexSize[hexSize.size()-1] == ' ' || hexSize[hexSize.size()-1] == '\t'))
    hexSize.erase(hexSize.size()-1);

char* endptr = NULL;
size_t chunkSize = std::strtoul(hexSize.c_str(), &endptr, 16);
if (endptr == hexSize.c_str() || *endptr != '\0')
    return false;
```

---

## 6. Bug: Dead members `_raw` and `_headersParsed`

### The problem

`httpRequest.hpp` declares:

```cpp
std::string _raw;
bool        _headersParsed;
```

Neither is ever assigned a meaningful value in `httpRequest.cpp`. `_raw` is never set.
`_headersParsed` is never read. They waste memory on every request and confuse anyone
reading the code.

### The fix

**Option A — Remove them entirely** (recommended):

```cpp
// httpRequest.hpp — delete these two members:
// std::string _raw;
// bool        _headersParsed;
```

Also remove them from the constructor initialiser list in `httpRequest.cpp`:

```cpp
HttpRequest::HttpRequest()
    : _method(""),
      _path(""),
      _version(""),
      _body(""),
      _contentLength(0),
      _hasContentLength(false)
      // _headersParsed removed
{
}
```

**Option B — Use `_headersParsed` as a guard** if you ever want to call `parse()` once
only and cheaply check afterwards:

```cpp
bool HttpRequest::isComplete() const
{
    if (!_headersParsed)
        return false;
    if (!_hasContentLength)
        return true;
    return _body.size() == _contentLength;
}

// At the end of parse(), set:
_headersParsed = true;
```

This is more defensive and slightly useful, but not required. Option A is simpler.

---

## 7. Design gap: Config not connected to routing

### The problem

`FileHandler::resolvePath()` hardcodes `"./www"`:

```cpp
std::string FileHandler::resolvePath(const std::string& urlPath)
{
    if (urlPath == "/")
        return "./www/index.html";       // hardcoded
    return "./www" + urlPath;            // hardcoded
}
```

`handlePost` hardcodes `"./www/upload.txt"`.

Your dummy config sets `server1.setRoot("./www1")` — which does not match `./www`. If
an evaluator tests with a different root, everything 404s silently.

### The fix

Pass the matched `ServerConfig` into `routeRequest` and `FileHandler`.

**Step 1 — Add a `findServerConfig` helper to `Engine`:**

```cpp
// engine.hpp
const ServerConfig* findServerConfig(const std::string& host, int port) const;
```

```cpp
// engine.cpp
const ServerConfig* Engine::findServerConfig(const std::string& host, int port) const
{
    const std::vector<ServerConfig>& servers = _config.getServers();
    for (size_t i = 0; i < servers.size(); ++i)
    {
        if (servers[i].getHost() == host && servers[i].getPort() == port)
            return &servers[i];
    }
    return NULL;
}
```

**Step 2 — Update `routeRequest` signature:**

```cpp
std::string routeRequest(const HttpRequest& request, bool shouldClose, const ServerConfig& serverConfig);
```

**Step 3 — Update `FileHandler::resolvePath` to accept the root:**

```cpp
// FileHandler.hpp
static std::string resolvePath(const std::string& urlPath, const std::string& root, const std::string& index);
```

```cpp
// FileHandler.cpp
std::string FileHandler::resolvePath(const std::string& urlPath, const std::string& root, const std::string& index)
{
    if (urlPath == "/")
        return root + "/" + index;
    return root + urlPath;
}
```

**Step 4 — Update `routeRequest` body:**

```cpp
std::string Engine::routeRequest(const HttpRequest& request, bool shouldClose, const ServerConfig& serverConfig)
{
    const std::string& root  = serverConfig.getRoot();
    const std::string& index = serverConfig.getIndex();

    if (request.getMethod() == "GET")
    {
        if (request.getPath().find("..") != std::string::npos)
            return buildResponse("403 Forbidden", "Forbidden", "text/plain", shouldClose);

        std::string path = FileHandler::resolvePath(request.getPath(), root, index);
        struct stat s;
        if (stat(path.c_str(), &s) == 0 && S_ISDIR(s.st_mode))
            path += "/" + index;

        if (!FileHandler::fileExists(path))
            return buildResponse("404 Not Found", "Not Found", "text/plain", shouldClose);

        std::string content = FileHandler::readFile(path);
        std::string mime = FileHandler::getMimeType(path);
        return buildResponse("200 OK", content, mime, shouldClose);
    }
    if (request.getMethod() == "POST")
        return handlePost(request, shouldClose, serverConfig);

    return buildResponse("405 Method Not Allowed", "Method Not Allowed", "text/plain", shouldClose);
}
```

**Step 5 — Update `handlePost` to use upload path from config:**

```cpp
std::string Engine::handlePost(const HttpRequest& request, bool shouldClose, const ServerConfig& serverConfig)
{
    // Find the matching location for the request path
    const std::vector<LocationConfig>& locations = serverConfig.getLocations();
    const LocationConfig* matchedLoc = NULL;
    for (size_t i = 0; i < locations.size(); ++i)
    {
        if (request.getPath().find(locations[i].getPath()) == 0)
        {
            matchedLoc = &locations[i];
            break;
        }
    }

    std::string uploadPath = serverConfig.getRoot() + "/upload.txt"; // fallback
    if (matchedLoc && matchedLoc->isUploadEnabled() && !matchedLoc->getUploadPath().empty())
        uploadPath = matchedLoc->getUploadPath() + "/upload.txt";

    const std::string& body = request.getBody();
    int fd = open(uploadPath.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0)
        return buildResponse("500 Internal Server Error", "Open Failed", "text/plain", shouldClose);

    size_t total = 0;
    while (total < body.size())
    {
        ssize_t written = write(fd, body.data() + total, body.size() - total);
        if (written <= 0)
        {
            close(fd);
            return buildResponse("500 Internal Server Error", "Write failed", "text/plain", shouldClose);
        }
        total += written;
    }
    close(fd);
    return buildResponse("201 Created", "Upload OK", "text/plain", shouldClose);
}
```

> Note: successful file upload should return `201 Created`, not `200 OK`.

---

## 8. Design gap: Allowed methods not enforced

### The problem

`loc1` allows `GET` and `POST`. `loc3` allows only `GET`. None of this is checked
anywhere before `routeRequest` dispatches to the handler.

### The fix

Add a method check after locating the matched `LocationConfig` in `routeRequest`:

```cpp
// Helper function
static bool isMethodAllowed(const std::string& method, const LocationConfig& loc)
{
    const std::vector<std::string>& allowed = loc.getAllowedMethods();
    if (allowed.empty())
        return true; // no restriction configured
    for (size_t i = 0; i < allowed.size(); ++i)
    {
        if (allowed[i] == method)
            return true;
    }
    return false;
}
```

```cpp
std::string Engine::routeRequest(const HttpRequest& request, bool shouldClose, const ServerConfig& serverConfig)
{
    // Find matching location
    const std::vector<LocationConfig>& locations = serverConfig.getLocations();
    const LocationConfig* matchedLoc = NULL;
    for (size_t i = 0; i < locations.size(); ++i)
    {
        if (request.getPath().find(locations[i].getPath()) == 0)
        {
            matchedLoc = &locations[i];
            break;
        }
    }

    // Enforce allowed methods
    if (matchedLoc && !isMethodAllowed(request.getMethod(), *matchedLoc))
    {
        return buildResponse("405 Method Not Allowed", "Method Not Allowed", "text/plain", shouldClose);
    }

    // ... rest of routing ...
}
```

---

## 9. Design gap: Max body size not from config

### The problem

The 1MB cap is hardcoded in `httpRequest.cpp`:

```cpp
if (_contentLength > 1000000)
    throw std::runtime_error("Body too large");
```

`MAX_REQUEST_SIZE` in `Webserv.hpp` is `1000000`. There is no field in `ServerConfig`
for a configurable body size limit.

### The fix

**Step 1 — Add `_maxBodySize` to `ServerConfig`:**

```cpp
// ServerConfig.hpp
private:
    size_t _maxBodySize;

public:
    void   setMaxBodySize(size_t size);
    size_t getMaxBodySize() const;
    bool   hasMaxBodySize() const;
```

```cpp
// ServerConfig.cpp
ServerConfig::ServerConfig()
    : _host(""), _port(0), _serverName(""), _root(""), _index(""),
      _maxBodySize(1000000), _locations()
{}

void ServerConfig::setMaxBodySize(size_t size) { _maxBodySize = size; }
size_t ServerConfig::getMaxBodySize() const    { return _maxBodySize; }
```

**Step 2 — Pass the limit into `HttpRequest::parse()`:**

```cpp
// httpRequest.hpp
void parse(const std::string& rawRequest, size_t maxBodySize = 1000000);
```

```cpp
// httpRequest.cpp — replace hardcoded check:
if (_hasContentLength && _contentLength > maxBodySize)
    throw std::runtime_error("Body too large");
```

**Step 3 — Pass it from the engine:**

```cpp
// in handleClientRequest, when calling parse:
request.parse(rawRequest, serverConfig.getMaxBodySize());
```

---

## 10. Design gap: Timeout does not apply to WRITING connections

### The problem

The timeout check lives inside `processIncomingData`. Connections in `WRITING` state
are never registered in `readSet`, so `processIncomingData` never iterates over them,
and they never time out. A client that opens a connection, receives the response header,
and then stops reading will hold the connection open forever.

### The fix

Move the timeout check to a dedicated method called from `run()` before
`processIncomingData`:

```cpp
// engine.hpp
void checkTimeouts();
```

```cpp
void Engine::checkTimeouts()
{
    time_t now = std::time(NULL);

    for (std::map<int, Connection*>::iterator it = _clientConnections.begin();
         it != _clientConnections.end();)
    {
        Connection* conn = it->second;
        time_t elapsed = now - conn->getLastActivity();

        bool timedOut = false;

        if (conn->getRequestState() == Connection::READING_HEADERS && elapsed > 5)
        {
            std::cout << "Header timeout fd=" << it->first << std::endl;
            timedOut = true;
        }
        else if (elapsed > 30)
        {
            // Covers both READING_BODY and WRITING states
            std::cout << "Connection timeout fd=" << it->first << std::endl;
            timedOut = true;
        }

        if (timedOut)
        {
            delete conn;
            _clientConnections.erase(it++);
        }
        else
            ++it;
    }
}
```

```cpp
// engine.cpp — in run():
void Engine::run()
{
    while (true)
    {
        // ... fd_set setup ...

        int readyFdCount = select(maxFd + 1, &readSet, &writeSet, NULL, &timeout);
        if (readyFdCount < 0)
        {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }

        checkTimeouts();                          // <-- add this
        acceptPendingClientConnections(readSet);
        processIncomingData(readSet);
        processOutgoingData(writeSet);
    }
}
```

Also remove the timeout logic from inside `processIncomingData` since it is now handled
centrally.

---

## 11. Missing: DELETE method

### The problem

Most 42 webserv rubrics require DELETE. Your `routeRequest` falls through to 405 for
any non-GET, non-POST method, which gives the correct status code but not for the right
reason. Confirm in your subject PDF whether DELETE is required.

### The fix

```cpp
// In routeRequest, after the POST branch:
if (request.getMethod() == "DELETE")
    return handleDelete(request, shouldClose, serverConfig);

return buildResponse("405 Method Not Allowed", "Method Not Allowed", "text/plain", shouldClose);
```

```cpp
// engine.hpp
std::string handleDelete(const HttpRequest& request, bool shouldClose, const ServerConfig& serverConfig);
```

```cpp
std::string Engine::handleDelete(const HttpRequest& request, bool shouldClose, const ServerConfig& serverConfig)
{
    if (request.getPath().find("..") != std::string::npos)
        return buildResponse("403 Forbidden", "Forbidden", "text/plain", shouldClose);

    std::string path = FileHandler::resolvePath(request.getPath(), serverConfig.getRoot(), serverConfig.getIndex());

    if (!FileHandler::fileExists(path))
        return buildResponse("404 Not Found", "Not Found", "text/plain", shouldClose);

    if (std::remove(path.c_str()) != 0)
        return buildResponse("500 Internal Server Error", "Delete failed", "text/plain", shouldClose);

    return buildResponse("204 No Content", "", "text/plain", shouldClose);
}
```

> `204 No Content` is the correct status for a successful DELETE. The body must be
> empty and `Content-Length: 0`.

---

## 12. Missing: Autoindex (directory listing)

### The problem

When a directory is requested and there is no `index.html`, the server currently returns
404. The subject usually requires a generated directory listing when `autoindex on` is
configured for that location.

### The fix

**Step 1 — Add `_autoindex` to `LocationConfig`:**

```cpp
// LocationConfig.hpp
private:
    bool _autoindex;

public:
    void setAutoindex(bool enabled);
    bool isAutoindex() const;
```

```cpp
// LocationConfig.cpp
LocationConfig::LocationConfig()
    : _path(""), _allowedMethods(), _root(""),
      _uploadEnabled(false), _uploadPath(""),
      _autoindex(false)
{}

void LocationConfig::setAutoindex(bool enabled) { _autoindex = enabled; }
bool LocationConfig::isAutoindex() const        { return _autoindex; }
```

**Step 2 — Add `generateDirectoryListing` to `FileHandler`:**

```cpp
// FileHandler.hpp
static std::string generateDirectoryListing(const std::string& urlPath, const std::string& fsPath);
```

```cpp
// FileHandler.cpp
#include <dirent.h>

std::string FileHandler::generateDirectoryListing(const std::string& urlPath, const std::string& fsPath)
{
    DIR* dir = opendir(fsPath.c_str());
    if (!dir)
        return "";

    std::string body = "<html><head><title>Index of " + urlPath + "</title></head><body>";
    body += "<h1>Index of " + urlPath + "</h1><hr><pre>";

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL)
    {
        std::string name = entry->d_name;
        if (name == ".")
            continue;
        body += "<a href=\"" + urlPath;
        if (urlPath[urlPath.size() - 1] != '/')
            body += "/";
        body += name + "\">" + name + "</a>\n";
    }
    closedir(dir);
    body += "</pre><hr></body></html>";
    return body;
}
```

**Step 3 — Use it in `routeRequest`:**

```cpp
// Inside the GET branch, where you currently detect a directory:
if (stat(path.c_str(), &s) == 0 && S_ISDIR(s.st_mode))
{
    std::string indexPath = path + "/" + index;
    if (FileHandler::fileExists(indexPath))
    {
        path = indexPath;
    }
    else if (matchedLoc && matchedLoc->isAutoindex())
    {
        std::string listing = FileHandler::generateDirectoryListing(request.getPath(), path);
        if (listing.empty())
            return buildResponse("500 Internal Server Error", "Cannot read directory", "text/plain", shouldClose);
        return buildResponse("200 OK", listing, "text/html", shouldClose);
    }
    else
    {
        return buildResponse("403 Forbidden", "Directory listing disabled", "text/plain", shouldClose);
    }
}
```

---

## 13. Missing: Custom error pages from config

### The problem

All error responses currently return an inline plain-text body. Most webserv rubrics
require that if the config specifies a custom error page (e.g. `error_page 404 /404.html`),
that file is served instead.

### The fix

**Step 1 — Add error page map to `ServerConfig`:**

```cpp
// ServerConfig.hpp
#include <map>

private:
    std::map<int, std::string> _errorPages;

public:
    void               addErrorPage(int code, const std::string& path);
    const std::string* getErrorPage(int code) const;
```

```cpp
// ServerConfig.cpp
void ServerConfig::addErrorPage(int code, const std::string& path)
{
    _errorPages[code] = path;
}

const std::string* ServerConfig::getErrorPage(int code) const
{
    std::map<int, std::string>::const_iterator it = _errorPages.find(code);
    if (it == _errorPages.end())
        return NULL;
    return &it->second;
}
```

**Step 2 — Add a helper to `Engine` that reads the error page or falls back:**

```cpp
// engine.hpp
std::string buildErrorResponse(int code, const std::string& defaultMsg, bool shouldClose, const ServerConfig* serverConfig);
```

```cpp
std::string Engine::buildErrorResponse(int code, const std::string& defaultMsg, bool shouldClose, const ServerConfig* serverConfig)
{
    if (serverConfig)
    {
        const std::string* pagePath = serverConfig->getErrorPage(code);
        if (pagePath)
        {
            std::string fullPath = serverConfig->getRoot() + *pagePath;
            if (FileHandler::fileExists(fullPath))
            {
                std::string body = FileHandler::readFile(fullPath);
                std::ostringstream status;
                status << code << " " << defaultMsg;
                return buildResponse(status.str(), body, "text/html", shouldClose);
            }
        }
    }
    std::ostringstream status;
    status << code << " " << defaultMsg;
    return buildResponse(status.str(), defaultMsg, "text/plain", shouldClose);
}
```

**Step 3 — Replace `build404Response()`, etc. with `buildErrorResponse()`:**

```cpp
return buildErrorResponse(404, "Not Found", shouldClose, &serverConfig);
return buildErrorResponse(403, "Forbidden", shouldClose, &serverConfig);
return buildErrorResponse(405, "Method Not Allowed", shouldClose, &serverConfig);
```

**Step 4 — Register custom error pages in your dummy config:**

```cpp
// dummyConfigFiles.cpp
server1.addErrorPage(404, "/404.html");
server1.addErrorPage(500, "/500.html");
```

Then create `./www1/404.html` and `./www1/500.html`.

---

## 14. Minor: Console spam and dead code cleanup

### Items to fix before evaluation

**Remove select spam from `engine.cpp`:**

```cpp
// DELETE this line in run():
std::cout << "select return= " << readyFdCount << std::endl;
```

**Remove dummy config debug print from `dummyConfigFiles.cpp`:**

```cpp
// DELETE this line in initDummy():
std::cout << "printing server" << std::endl;
```

**Remove the `about to send` debug line in `processOutgoingData`:**

```cpp
// DELETE:
std::cout << "about to send on fd=" << clientFd << std::endl;
```

**`Connection::headerComplete()` is duplicated logic.** Either use it consistently or
remove it:

```cpp
// If unused, delete from Connection.hpp and connection.cpp:
// bool headerComplete() const;
```

**`main.cpp` accepts `argc == 2` but ignores `argv[1]`.** Add a note or stub:

```cpp
if (argc == 2)
{
    // TODO: parse argv[1] as config file path
    // For now, dummy config is always used
    (void)argv[1];
}
```

**`handlePost` uses `O_TRUNC` on every POST** — it destroys the previous upload. If
your subject requires appending or multi-file upload support, revisit this. At minimum,
note it in your AMENDMENT.md.

---

## Summary priority order

| Priority | Item |
|---|---|
| Fix now | §1 — Iterator double-advance (crashes under load) |
| Fix now | §2 — `build400Response` linker error |
| Fix now | §7 — Config root not connected (silent wrong directory) |
| Before eval | §3 — `shouldClose` hardcoded false (wrong Connection header) |
| Before eval | §8 — Allowed methods not enforced |
| Before eval | §10 — WRITING connections never time out |
| Before eval | §11 — DELETE method (check your subject) |
| Before eval | §12 — Autoindex (check your subject) |
| Before eval | §13 — Custom error pages |
| Before eval | §14 — Remove all debug console output |
| Polish | §4 — Duplicate Content-Length rejection |
| Polish | §5 — Chunk-extensions stripping |
| Polish | §6 — Remove dead members |
| Polish | §9 — Max body size from config |