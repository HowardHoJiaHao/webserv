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

static bool isCgiRequestForLocation(const std::string& path, const LocationConfig& location)
{
	if (!location.isCgiEnabled())
		return false;

	const std::vector<std::string>& exts = location.getCgiExtensions();
	if (exts.empty())
		return true;

	for (size_t i = 0; i < exts.size(); ++i)
	{
		const std::string normalized = normalizeCgiExtension(exts[i]);
		if (!normalized.empty() && hasSuffix(path, normalized))
			return true;
	}
	return false;
}

static const char* kSessionCookieName = "webservsid";
static const time_t kSessionTtlSeconds = 3600;

void Engine::pruneExpiredSessions(time_t now)
{
	for (std::map<std::string, time_t>::iterator it = _sessions.begin(); it != _sessions.end();)
	{
		if (now - it->second > kSessionTtlSeconds)
			_sessions.erase(it++);
		else
			++it;
	}
}

std::string Engine::generateSessionId(time_t now)
{
	++_sessionCounter;
	std::ostringstream oss;
	oss << std::hex
		<< static_cast<unsigned long>(now)
		<< static_cast<unsigned long>(::getpid())
		<< static_cast<unsigned long>(std::rand())
		<< _sessionCounter;
	return oss.str();
}

std::string Engine::ensureSessionCookieHeader(const HttpRequest& request)
{
	time_t now = std::time(NULL);
	pruneExpiredSessions(now);

	const std::string* existingSessionId = request.getCookie(kSessionCookieName);
	if (existingSessionId != NULL && !existingSessionId->empty())
	{
		std::map<std::string, time_t>::iterator it = _sessions.find(*existingSessionId);
		if (it != _sessions.end())
		{
			it->second = now;
			return "";
		}
	}

	std::string sessionId;
	do
	{
		sessionId = generateSessionId(now);
	}
	while (_sessions.find(sessionId) != _sessions.end());

	_sessions[sessionId] = now;

	std::ostringstream header;
	header << "Set-Cookie: " << kSessionCookieName << "=" << sessionId
		<< "; Path=/; Max-Age=" << kSessionTtlSeconds << "; HttpOnly";
	return header.str();
}



bool Engine::handleRequestExtraction(Connection* conn, std::string& rawRequest, bool& extracted)
{
	extracted = false;
	std::string& readBuffer = conn->getReadBuffer();
	bool isInvalidContentLength = false;
	//extract one complete HTTP request from buffer into rawRequest, as reference
	if (!validateAndExtractRequestFromBuffer(readBuffer, rawRequest, &isInvalidContentLength))
	{
		// broken header / invalid format
		if (isInvalidContentLength)
		{
			conn->setShouldClose(true);
			conn->setWriteBuffer(buildErrorResponse(400, conn->shouldClose(), conn->getServerConfig()));
			conn->setState(Connection::WRITING);
			return false;
		}
		// if not malformed and
		// incomplete extract Request, not enough of data extracted
		size_t headerEnd = readBuffer.find("\r\n\r\n");
		// incomplete headerEnd read
		if (headerEnd == std::string::npos)
			conn->setRequestState(Connection::READING_HEADERS);
		else
			conn->setRequestState(Connection::READING_BODY);
		return true;
	}
	// success case
	conn->setRequestState(Connection::COMPLETE);
	extracted = true;
	return true;
}

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
	catch (const std::exception& e)
	{
		conn->setShouldClose(true);
		if (std::string(e.what()) == "Body too large")
				conn->setWriteBuffer(buildErrorResponse(413, conn->shouldClose(), serverConfig));
		else
				conn->setWriteBuffer(buildErrorResponse(400, conn->shouldClose(), serverConfig));
		conn->setState(Connection::WRITING);
		return false;
	}

	return true;
}





bool Engine::handleRequestExecution(Connection* conn, const HttpRequest& request, const ServerConfig* serverConfig, const LocationConfig* location, bool shouldClose, bool& producedResponse)
{
	if (serverConfig == NULL)
	{
		conn->setShouldClose(true);
		conn->setWriteBuffer(buildErrorResponse(500, conn->shouldClose(), conn->getServerConfig()));
		conn->setState(Connection::WRITING);
		return false;
	}

	std::string sessionSetCookieHeader = ensureSessionCookieHeader(request);
	if (sessionSetCookieHeader.empty())
		conn->clearPendingSetCookieHeader();
	else
		conn->setPendingSetCookieHeader(sessionSetCookieHeader);

	if (!isMethodAllowed(request.getMethod(), location))
	{
		std::string response = buildErrorResponse(405, conn->shouldClose(), serverConfig, methodNotAllowedHeaders(location));
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
		if (!launchCGI(conn, request, *serverConfig, location, conn->shouldClose()))
		{
			if (!conn->getPendingSetCookieHeader().empty())
			{
				conn->setWriteBuffer(appendHeaderToResponse(conn->getWriteBuffer(), conn->getPendingSetCookieHeader()));
				conn->clearPendingSetCookieHeader();
			}
			conn->setState(Connection::WRITING);
		}
		return false;
	}

	std::string response = routeRequest(request, shouldClose, *serverConfig, location);
	if (!conn->getPendingSetCookieHeader().empty())
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
// headerEnd starts with no position

bool Engine::attemptIncomingHeader(Connection* conn, const char* buffer, ssize_t bytes, size_t& headerEnd)
{
	conn->appendToHeaderBuffer(buffer, bytes);
	conn->updateLastActivity();

	std::string& readBuffer = conn->getReadBuffer();
	headerEnd = readBuffer.find("\r\n\r\n");
	// sometimes request size is too huge, then i reject it
	if (!enforceRequestSizeLimits(conn, headerEnd))
		return false;

	// no \r\n\r\n yet, headers incomplete
	if (headerEnd == std::string::npos)
		conn->setRequestState(Connection::READING_HEADERS);
	// found that \r\n\r\n
	else
		conn->setRequestState(Connection::READING_BODY); // should it be reading completion
	return true;
}

bool Engine::processBufferedRequests(Connection* conn, bool& producedResponse)
{
	std::string rawRequest;

	while (true)
	{
		bool isExtracted = false;
		if (!handleRequestExtraction(conn, rawRequest, isExtracted))
			return false;
		if (!isExtracted)
			break;

		//rawRequest string -> structured data
		HttpRequest request;
		const ServerConfig* serverConfig = conn->getServerConfig();
		if (!handleRequestParsing(conn, rawRequest, request, serverConfig))
			return false;

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
	// get MaxBodySize from the config
	size_t maxBodySizeLimit = MAX_REQUEST_SIZE;
	if (serverConfig != NULL)
		maxBodySizeLimit = serverConfig->getMaxBodySize();
	size_t maxBufferedRequestSize = maxBodySizeLimit + maxHeaderSize;

	// if the header is gabbage without \r\n\r\n
	if (headerEndPos == std::string::npos && requestBuffer.size() > maxHeaderSize)
	{
		conn->setShouldClose(true);
		requestBuffer.clear();
		conn->setWriteBuffer(buildErrorResponse(413, conn->shouldClose(), serverConfig));
		conn->setState(Connection::WRITING);
		return false;
	}

	//total buffer is too big
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

void Engine::handleClientRequest(Connection* conn, const char* buffer, ssize_t bytes)
{
	size_t headerEnd = std::string::npos;
	if (!attemptIncomingHeader(conn, buffer, bytes, headerEnd))
		return;

	bool producedResponse = false;
	if (!processBufferedRequests(conn, producedResponse))
		return;

	if (producedResponse)
		conn->setState(Connection::WRITING);
	else
		conn->setState(Connection::READING);
}
