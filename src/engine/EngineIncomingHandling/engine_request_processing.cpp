#include "engine.hpp"
#include "requestValidator.hpp"
#include "Webserv.hpp"

#include <cstdlib>



static bool hasSuffix(const std::string& value, const std::string& suffix)
{
	if (value.size() < suffix.size())
		return false;
	return value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static std::string normalizeCgiExtension(const std::string& ext)
{
	if (ext.empty())
		return ext;
	if (ext[0] == '.')
		return ext;
	return "." + ext;
}
//refer to config
// location /cgi-bin
//{
//     cgi_enabled on;
//     cgi_ext .py .pl;
// }
// is cgi enabled on and the extension matched (config vs request: GET /cgi-bin/test.py)
static bool isCgiRequestForLocation(const std::string& path, const LocationConfig& location)
{
	if (!location.isCgiEnabled())
		return false;
	// if no cgi extension is set, then no cgi request
	const std::vector<std::string>& exts = location.getCgiExtensions();
	if (exts.empty())
		return false;

	for (size_t i = 0; i < exts.size(); ++i)
	{
		const std::string normalized = normalizeCgiExtension(exts[i]);
		if (!normalized.empty() && hasSuffix(path, normalized))
			return true;
	}
	return false;
}

// there is a few function are using these static variable

// this is key -> webservsid=abc123 <- value
static const std::string ConstantSessionCookieName = "webservsid";
static const time_t ConstantSessionTimeToLiveSeconds = 3600;

// if a session lived too long since last used, longer than TTL, then it should die
// session
// _sessions
// ├── "abc123" → 1712500000
// ├── "xyz789" → 1712500100
// ├── "k9lmno" → 1712500200
void Engine::pruneExpiredSessions(time_t now)
{
	// go throught each session stored in _sessions
	for (std::map<std::string, time_t>::iterator it = _sessions.begin(); it != _sessions.end();)
	{
		if (now - it->second > ConstantSessionTimeToLiveSeconds)
			_sessions.erase(it++);
		else
			++it;
	}
}

// time now + rand digit + sessionCounter, everything in hex
std::string Engine::generateSessionId(time_t now)
{
	++_sessionCounter;
	std::ostringstream oss;
	oss << std::hex
		<< static_cast<unsigned long>(now)
		<< static_cast<unsigned long>(std::rand())
		<< _sessionCounter;
	return oss.str();
}

// cannot return reference because i might return ""
//
// session id: cookieSessionId -> time
std::string Engine::ensureSessionCookieHeader(const HttpRequest& request)
{
	time_t now = std::time(NULL);
	pruneExpiredSessions(now);
	// the first time client request should have null existingSessionId 
	const std::string* existingSessionId = request.getCookieValue(ConstantSessionCookieName);

	// if the client session exist, refresh its last used time
	if (existingSessionId != NULL && !existingSessionId->empty())
	{
		std::map<std::string, time_t>::iterator it = _sessions.find(*existingSessionId);
		if (it != _sessions.end())
		{
			it->second = now;
			return "";
		}
	}

	// if above "if" statement failed, session is not found in _session
	// generate a unique session id that does not already exist in _sessions
	std::string sessionId;
	while (true)
	{
		sessionId = generateSessionId(now);
		if (_sessions.find(sessionId) == _sessions.end())
			break;
	}
	_sessions[sessionId] = now;

	// response to client
	// this is RFC standard 6265
	// Set-Cookie: webservsid=69d75a62104f54011; Path=/; Max-Age=3600; HttpOnly
	std::ostringstream SetCookieHeader;
	SetCookieHeader << "Set-Cookie: " << ConstantSessionCookieName << "=" << sessionId
		<< "; Path=/; Max-Age=" << ConstantSessionTimeToLiveSeconds << "; HttpOnly";
	return SetCookieHeader.str();
}

//	==========		INPUT		==========
// rawRequest is empty when it was started
// extracted is a flag that is initialized as falsed
// conn has the read buffer to be processed
bool Engine::handleRequestExtraction(Connection* conn, std::string& rawRequest, bool& extracted)
{
	extracted = false;
	std::string& readBuffer = conn->getReadBuffer();
	bool isInvalidContentLength = false;
	//extract one complete HTTP request from buffer into rawRequest, as reference
	// if header complete, body complete(post only), valid format: return true, the if block skipped
	if (!httpRequestCompletenessChecking(readBuffer, rawRequest, &isInvalidContentLength))
	{
		// broken header / invalid format
		if (isInvalidContentLength)
		{
			conn->setShouldClose(true);
			conn->setWriteBuffer(buildErrorResponse(400, conn->shouldClose(), conn->getServerConfig()));
			conn->setState(Connection::WRITING);
			return false;
		}

		// ======= is the header complete? =======
		// if not, next loop will continue read header
		size_t headerEnd = readBuffer.find("\r\n\r\n");
		// incomplete headerEnd read
		if (headerEnd == std::string::npos)
			conn->setRequestState(Connection::READING_HEADERS);
		else
			conn->setRequestState(Connection::READING_BODY);
		// headerEnd is complete, but body might not, return will cause loop to continue and wait few more bytes by recv()
		return true;
	}
	// success case
	conn->setRequestState(Connection::READING_BODY);
	extracted = true;
	return true;
}

// conn is clientSocketConnection: has state, buffer, flags
// rawRequest is the extracted request from readBuffer
// request is the parsed request (structured data object)
// serverConfig is the server config that passed as program parameter
bool Engine::handleRequestParsing(Connection* conn, const std::string& rawRequest, HttpRequest& request, const ServerConfig* serverConfig)
{
	// if the program is run without any config, use marco
	// else using config max body size
	size_t maxBodySize = MAX_REQUEST_SIZE;
	if (serverConfig != NULL)
		maxBodySize = serverConfig->getMaxBodySize();

	try
	{
		request.parse(rawRequest, maxBodySize);
	}
	// 413 and 400 is specific to parsing(), with exception flow
	catch (const std::exception& e)
	{
		conn->setShouldClose(true);
		// specific error
		if (std::string(e.what()) == "Body too large")
				conn->setWriteBuffer(buildErrorResponse(413, conn->shouldClose(), serverConfig));
		else
		//bad client input
				conn->setWriteBuffer(buildErrorResponse(400, conn->shouldClose(), serverConfig));
		conn->setState(Connection::WRITING);
		return false;
	}

	return true;
}

bool Engine::handleRequestExecution(Connection* conn, const HttpRequest& request, const ServerConfig* serverConfig, const LocationConfig* location, bool shouldClose, bool& producedResponse)
{
	//defensive
	if (serverConfig == NULL)
	{
		conn->setShouldClose(true);
		conn->setWriteBuffer(buildErrorResponse(500, conn->shouldClose(), conn->getServerConfig()));
		conn->setState(Connection::WRITING);
		return false;
	}

	// sessionSetCookieHeader: webservsid=69d75a62104f54011; Path=/; Max-Age=3600; HttpOnly
	std::string sessionSetCookieHeader = ensureSessionCookieHeader(request);
	// in the previous function, if valid session is found in _session, it will return ""
	// means no new cookie needs to be sent, else will set new cookie
	if (sessionSetCookieHeader.empty())
		conn->clearPendingSetCookieHeader();
	else
		conn->setPendingSetCookieHeader(sessionSetCookieHeader);

	if (!isMethodAllowed(request.getMethod(), location))
	{
		std::string response = buildErrorResponse(405, conn->shouldClose(), serverConfig, methodNotAllowedHeaders(location));
		// if there is a pending cookie header, append it to the response
		if (!conn->getPendingSetCookieHeader().empty())
		{
			response = appendHeaderToResponse(response, conn->getPendingSetCookieHeader());
			conn->clearPendingSetCookieHeader();
		}
		conn->setWriteBuffer(response);
		conn->setState(Connection::WRITING);
		return false;
	}

	if (location != NULL && isCgiRequestForLocation(request.getPath(), *location))
	{
		// start cgi process, forking the server, setting up pipes, preparing the connection to communicate with external program
		if (!launchCGI(conn, request, *serverConfig, location, conn->shouldClose()))
		{
			// if cgi launching failed, send existing response with cookie, switch to writing state
			if (!conn->getPendingSetCookieHeader().empty())
			{
				conn->setWriteBuffer(appendHeaderToResponse(conn->getWriteBuffer(), conn->getPendingSetCookieHeader()));
				conn->clearPendingSetCookieHeader();
			}
			conn->setState(Connection::WRITING);
		}
		return false;
	}
	// the moment the first if block is true, then eventually it will enter this false return and it wont go to the normal request (next block of code)

	std::string response = routeRequest(request, shouldClose, *serverConfig, location);
	// if cookie string(_pendingSetCookieHeader) is not empty
	if (conn->getPendingSetCookieHeader().size() > 0)
	{
		response = appendHeaderToResponse(response, conn->getPendingSetCookieHeader());
		conn->clearPendingSetCookieHeader();
	}
	conn->appendToWriteBuffer(response);
	producedResponse = true;
	return true;
}

// buffer is raw bytes received from client socket via recv()
// example 
// GET /index.html HTTP/1.1\r\n <-_readBuffer will store it
// Host: localhost:8080\r\n
// User-Agent: curl/7.81.0\r\n
// Accept: */*\r\n
// \r\n

// conn is client socket connection

bool Engine::attemptIncomingHeader(Connection* conn, const char* buffer, ssize_t bytes)
{
	// store the buffer that output from the recv()
	conn->appendToHeaderBuffer(buffer, bytes);
	// for timeout control
	conn->updateLastActivity();

	std::string& readBuffer = conn->getReadBuffer();
	size_t headerEnd = readBuffer.find("\r\n\r\n");
	// sometimes if header request size is too huge, then i reject it
	if (!enforceRequestSizeLimits(conn, headerEnd))
		return false;

	// no \r\n\r\n yet, headers incomplete
	if (headerEnd == std::string::npos)
		conn->setRequestState(Connection::READING_HEADERS);
	// found that \r\n\r\n
	else
		conn->setRequestState(Connection::READING_BODY);
	return true;
}

// validate -> extract -> parse -> execute -> response
bool Engine::processBufferedRequests(Connection* conn, bool& producedResponse)
{
	std::string rawRequest;

	while (true)
	{
		// this block handle http completeness, if incomplete happen, then it break here, and return true, return to previous call, then move to next connection
		bool isExtracted = false;
		if (!handleRequestExtraction(conn, rawRequest, isExtracted))
			return false;
		// this block expecting a complete full http request, get / delete no need body, post need body by matching content length
		if (!isExtracted)
			break;

		//rawRequest string -> structured data
		HttpRequest request;
		const ServerConfig* serverConfig = conn->getServerConfig();
		if (!handleRequestParsing(conn, rawRequest, request, serverConfig))
			return false;

		// return the pointer to the location config
		const LocationConfig* location = NULL;
		if (serverConfig != NULL)
			location = findBestLocation(*serverConfig, request.getPath());

		bool shouldClose = request.shouldCloseConnectionByHttpRules();
		if (shouldClose)
			conn->setShouldClose(true);

		if (!handleRequestExecution(conn, request, serverConfig, location, shouldClose, producedResponse))
			return false;
	}

	return true;
}

// requestBuffer = header + body
bool Engine::enforceRequestSizeLimits(Connection* conn, size_t headerEndPos)
{
	//hard cap header size <= 8kb
	const size_t maxHeaderSize = 8192;

	std::string& requestBuffer = conn->getReadBuffer();
	const ServerConfig* serverConfig = conn->getServerConfig();
	// get MaxBodySize from the config if i have
	size_t maxBodySizeLimit = MAX_REQUEST_SIZE;
	// define  max buffered request size for later 
	if (serverConfig != NULL)
		maxBodySizeLimit = serverConfig->getMaxBodySize();
	size_t maxBufferedRequestSize = maxBodySizeLimit + maxHeaderSize;

	// only related to header
	// if the header is gabbage without \r\n\r\n, \r\n\r\n was not found yet, buffer too big
	if (headerEndPos == std::string::npos && requestBuffer.size() > maxHeaderSize)
	{
		// reject 413...
		conn->setShouldClose(true);
		requestBuffer.clear();
		conn->setWriteBuffer(buildErrorResponse(413, conn->shouldClose(), serverConfig));
		conn->setState(Connection::WRITING);
		return false;
	}

	//the whole request including body (ofen post), total buffer is too big
	if (requestBuffer.size() > maxBufferedRequestSize)
	{
		conn->setShouldClose(true);
		requestBuffer.clear();
		conn->setWriteBuffer(buildErrorResponse(413, conn->shouldClose(), serverConfig));
		conn->setState(Connection::WRITING);
		return false;
	}
	// no \r\n\r\n, buffer not exceeding limit -> still return true
	return true;
}

// buffer is the data received from the client by using recv(), which returning bytes
void Engine::handleClientRequest(Connection* conn, const char* buffer, ssize_t bytes)
{
	// in this if condition, only way for it to return is request header too big, then return false
	// to check if the header is complete and ready for later
	if (!attemptIncomingHeader(conn, buffer, bytes))
		return;
	// the _readBuffer has the value, last activity updated, request is valid, safe to continue, not oversized (positive outcome)

	bool isProducedResponse = false;
	// basically here, given a request, it is validated, extracted, parsed, executed (get/post/delete), produce response
	if (!processBufferedRequests(conn, isProducedResponse))
		return;

	// response like 200, 404 that will sent to client, depends on response is produced or not
	if (isProducedResponse)
		conn->setState(Connection::WRITING);
	else
		conn->setState(Connection::READING);
}
