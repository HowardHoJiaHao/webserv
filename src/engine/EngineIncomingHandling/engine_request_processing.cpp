#include "engine.hpp"
#include "extractRequest.hpp"
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

bool Engine::prepareConnection(Connection* conn, const char* buffer, ssize_t bytes, size_t& headerEnd)
{
	conn->appendToReadBuffer(buffer, bytes);
	conn->updateActivity();

	std::string& readBuffer = conn->getReadBuffer();
	headerEnd = readBuffer.find("\r\n\r\n");
	if (!enforceRequestSizeLimits(conn, headerEnd))
		return false;

	if (headerEnd == std::string::npos)
		conn->setRequestState(Connection::READING_HEADERS);
	else
		conn->setRequestState(Connection::READING_BODY);
	return true;
}

bool Engine::processBufferedRequests(Connection* conn, bool& producedResponse)
{
	std::string& readBuffer = conn->getReadBuffer();
	std::string rawRequest;

	while (true)
	{
		bool malformed = false;
		if (!extractRequest(readBuffer, rawRequest, &malformed))
		{
			if (malformed)
			{
				conn->setShouldClose(true);
				conn->getWriteBuffer() = buildErrorResponse(400, conn->shouldClose(), NULL);
				conn->setState(Connection::WRITING);
				return false;
			}
			size_t headerEnd = readBuffer.find("\r\n\r\n");
			if (headerEnd == std::string::npos)
				conn->setRequestState(Connection::READING_HEADERS);
			else
				conn->setRequestState(Connection::READING_BODY);
			break;
		}
		conn->setRequestState(Connection::COMPLETE);

		HttpRequest request;
		std::vector<std::string> extraHeaders;
		const ServerConfig* defaultServer = findServerConfig(
			_clientListenEndpoints[conn->getFd()].first,
			_clientListenEndpoints[conn->getFd()].second
		);
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
				conn->getWriteBuffer() = buildErrorResponse(413, conn->shouldClose(), defaultServer);
			else
				conn->getWriteBuffer() = buildErrorResponse(400, conn->shouldClose(), defaultServer);
			conn->setState(Connection::WRITING);
			return false;
		}

		const ServerConfig* serverConfig = conn->getServerConfig();
		if (serverConfig != NULL && request.hasContentLength() && request.getContentLength() > serverConfig->getMaxBodySize())
		{
			conn->setShouldClose(true);
			conn->getWriteBuffer() = buildErrorResponse(413, conn->shouldClose(), serverConfig);
			conn->setState(Connection::WRITING);
			return false;
		}

		bool shouldClose = request.shouldCloseConnection();
		if (shouldClose)
			conn->setShouldClose(true);

		const ServerConfig* effectiveServer = defaultServer;
		if (serverConfig != NULL)
			effectiveServer = serverConfig;
		if (effectiveServer == NULL)
		{
			conn->setShouldClose(true);
			conn->getWriteBuffer() = buildErrorResponse(500, conn->shouldClose(), NULL);
			conn->setState(Connection::WRITING);
			return false;
		}

		const LocationConfig* matchedLocation = findBestLocation(*effectiveServer, request.getPath());
		if (!isMethodAllowed(request.getMethod(), matchedLocation))
		{
			conn->getWriteBuffer() = buildErrorResponse(405, conn->shouldClose(), effectiveServer, methodNotAllowedHeadersEngine(matchedLocation));
			conn->setState(Connection::WRITING);
			return false;
		}

		if (matchedLocation != NULL && isCgiRequestForLocation(request.getPath(), *matchedLocation))
		{
			if (!launchCGI(conn, request, *effectiveServer, conn->shouldClose()))
				conn->setState(Connection::WRITING);
			return false;
		}

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
		conn->getWriteBuffer() += response;
		producedResponse = true;
	}

	return true;
}

bool Engine::enforceRequestSizeLimits(Connection* conn, size_t headerEnd)
{
	const size_t maxHeaderSize = 8192;

	std::string& readBuffer = conn->getReadBuffer();
	const ServerConfig* defaultServerForLimit = findServerConfig(
		_clientListenEndpoints[conn->getFd()].first,
		_clientListenEndpoints[conn->getFd()].second
	);
	size_t maxBodySizeLimit = MAX_REQUEST_SIZE;
	if (defaultServerForLimit != NULL)
		maxBodySizeLimit = defaultServerForLimit->getMaxBodySize();
	size_t maxBufferedRequestSize = maxBodySizeLimit + maxHeaderSize;

	if (headerEnd == std::string::npos && readBuffer.size() > maxHeaderSize)
	{
		conn->setShouldClose(true);
		readBuffer.clear();
		conn->getWriteBuffer() = buildErrorResponse(413, conn->shouldClose(), defaultServerForLimit);
		conn->setState(Connection::WRITING);
		return false;
	}

	if (readBuffer.size() > maxBufferedRequestSize)
	{
		conn->setShouldClose(true);
		readBuffer.clear();
		conn->getWriteBuffer() = buildErrorResponse(413, conn->shouldClose(), defaultServerForLimit);
		conn->setState(Connection::WRITING);
		return false;
	}

	return true;
}

void Engine::handleClientRequest(Connection* conn, const char* buffer, ssize_t bytes)
{
	size_t headerEnd = std::string::npos;
	if (!prepareConnection(conn, buffer, bytes, headerEnd))
		return;

	bool producedResponse = false;
	if (!processBufferedRequests(conn, producedResponse))
		return;

	if (producedResponse)
		conn->setState(Connection::WRITING);
	else
		conn->setState(Connection::READING);
}
