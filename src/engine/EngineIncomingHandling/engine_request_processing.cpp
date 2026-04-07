#include "engine.hpp"
#include "requestValidator.hpp"
#include "Webserv.hpp"

#include <sstream>
#include <cstdlib>
#include <ctime>

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

static std::vector<std::string> methodNotAllowedHeadersEngine(const LocationConfig* location)
{
	std::string allowValue = "GET, POST, DELETE";
	if (location != NULL)
	{
		const std::vector<std::string>& allowed = location->getAllowedMethods();
		if (!allowed.empty())
		{
			allowValue.clear();
			for (size_t i = 0; i < allowed.size(); ++i)
			{
				if (i > 0)
					allowValue += ", ";
				allowValue += allowed[i];
			}
		}
	}

	std::vector<std::string> headers;
	headers.push_back("Allow: " + allowValue);
	return headers;
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
			conn->setWriteBuffer(buildErrorResponse(400, conn->shouldClose(), NULL));
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

bool Engine::handleRequestParsing(Connection* conn, const std::string& rawRequest, HttpRequest& request, const ServerConfig* defaultServer)
{
	// if the program is run without any config, use marco
	// else using config max body size
	size_t maxBodySize = MAX_REQUEST_SIZE;
	if (defaultServer != NULL)
		maxBodySize = defaultServer->getMaxBodySize();

	try
	{
		request.parse(rawRequest, maxBodySize);
	}
	catch (const std::exception& e)
	{
		conn->setShouldClose(true);
		if (std::string(e.what()) == "Body too large")
				conn->setWriteBuffer(buildErrorResponse(413, conn->shouldClose(), defaultServer));
		else
				conn->setWriteBuffer(buildErrorResponse(400, conn->shouldClose(), defaultServer));
		conn->setState(Connection::WRITING);
		return false;
	}

	return true;
}

bool Engine::enforceRequestBodySizeLimit(Connection* conn, const HttpRequest& request, const ServerConfig* serverConfig)
{
	if (serverConfig != NULL && request.hasContentLength() && request.getContentLength() > serverConfig->getMaxBodySize())
	{
		conn->setShouldClose(true);
		conn->setWriteBuffer(buildErrorResponse(413, conn->shouldClose(), serverConfig));
		conn->setState(Connection::WRITING);
		return false;
	}

	return true;
}

void Engine::handleSession(const HttpRequest& request, std::vector<std::string>& extraHeaders)
{
	std::string sessionId = request.getCookie("sessionId");

	if (sessionId.empty() || _sessions.find(sessionId) == _sessions.end())
	{
		std::stringstream ss;
		ss << std::rand() << std::time(NULL);
		sessionId = ss.str();

		_sessions[sessionId] = 1;
		extraHeaders.push_back("Set-Cookie: sessionId=" + sessionId + "; Path=/");
	}
	else
	{
		_sessions[sessionId]++;
	}
}

bool Engine::handleRequestExecution(Connection* conn, const HttpRequest& request, const ServerConfig* defaultServer, const ServerConfig* serverConfig, bool shouldClose, bool& producedResponse)
{
	const ServerConfig* effectiveServer = defaultServer;
	if (serverConfig != NULL)
		effectiveServer = serverConfig;
	if (effectiveServer == NULL)
	{
		conn->setShouldClose(true);
		conn->setWriteBuffer(buildErrorResponse(500, conn->shouldClose(), NULL));
		conn->setState(Connection::WRITING);
		return false;
	}

	const LocationConfig* matchedLocation = findBestLocation(*effectiveServer, request.getPath());
	if (!isMethodAllowed(request.getMethod(), matchedLocation))
	{
		conn->setWriteBuffer(buildErrorResponse(405, conn->shouldClose(), effectiveServer, methodNotAllowedHeadersEngine(matchedLocation)));
		conn->setState(Connection::WRITING);
		return false;
	}

	if (matchedLocation != NULL && isCgiRequestForLocation(request.getPath(), *matchedLocation))
	{
		if (!launchCGI(conn, request, *effectiveServer, conn->shouldClose()))
			conn->setState(Connection::WRITING);
		return false;
	}

	std::vector<std::string> extraHeaders;
	handleSession(request, extraHeaders);

	std::string response = routeRequest(request, shouldClose, *effectiveServer);
	if (!extraHeaders.empty())
	{
		size_t pos = response.find("\r\n\r\n");
		if (pos != std::string::npos)
		{
			std::string headerPart = response.substr(0, pos);
			std::string bodyPart = response.substr(pos);

			for (size_t i = 0; i < extraHeaders.size(); i++)
				headerPart += "\r\n" + extraHeaders[i];

			response = headerPart + bodyPart;
		}
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
		const ServerConfig* defaultServer = findServerConfig(
			_clientListenEndpoints[conn->getFd()].first,
			_clientListenEndpoints[conn->getFd()].second
		);
		if (!handleRequestParsing(conn, rawRequest, request, defaultServer))
			return false;

		const ServerConfig* serverConfig = conn->getServerConfig();
		if (!enforceRequestBodySizeLimit(conn, request, serverConfig))
			return false;

		bool shouldClose = request.shouldCloseConnectionByHttpRules();
		if (shouldClose)
			conn->setShouldClose(true);

		if (!handleRequestExecution(conn, request, defaultServer, serverConfig, shouldClose, producedResponse))
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
	// get the relevant config of the client listening port
	const ServerConfig* defaultServerForLimit = findServerConfig(
		_clientListenEndpoints[conn->getFd()].first,
		_clientListenEndpoints[conn->getFd()].second
	);
	// get MaxBodySize from the config
	size_t maxBodySizeLimit = MAX_REQUEST_SIZE;
	if (defaultServerForLimit != NULL)
		maxBodySizeLimit = defaultServerForLimit->getMaxBodySize();
	size_t maxBufferedRequestSize = maxBodySizeLimit + maxHeaderSize;

	// if the header is gabbage without \r\n\r\n
	if (headerEndPos == std::string::npos && requestBuffer.size() > maxHeaderSize)
	{
		conn->setShouldClose(true);
		requestBuffer.clear();
		conn->setWriteBuffer(buildErrorResponse(413, conn->shouldClose(), defaultServerForLimit));
		conn->setState(Connection::WRITING);
		return false;
	}

	//total buffer is too big
	if (requestBuffer.size() > maxBufferedRequestSize)
	{
		conn->setShouldClose(true);
		requestBuffer.clear();
		conn->setWriteBuffer(buildErrorResponse(413, conn->shouldClose(), defaultServerForLimit));
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
