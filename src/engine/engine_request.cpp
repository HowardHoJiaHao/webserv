#include "engine.hpp"
#include "engine_string_utils.hpp"
#include "extractRequest.hpp"
#include "FileHandler.hpp"
#include "Webserv.hpp"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cerrno>

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

static bool isUnsafeUploadFilenameEngine(const std::string& name)
{
	if (name.empty())
		return true;
	if (name == "." || name == "..")
		return true;
	if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos)
		return true;
	return false;
}

static std::string extractUploadFilenameEngine(const HttpRequest& request)
{
	const std::string* contentDisposition = request.getHeader("content-disposition");
	if (contentDisposition == NULL || contentDisposition->empty())
		return "";

	const std::string& headerValue = *contentDisposition;
	std::string lowered = toLowerAsciiEngine(headerValue);
	size_t keyPos = lowered.find("filename=");
	if (keyPos == std::string::npos)
		return "";

	size_t valueStart = keyPos + 9;
	if (valueStart >= headerValue.size())
		return "";

	size_t valueEnd = std::string::npos;
	if (headerValue[valueStart] == '"' || headerValue[valueStart] == '\'')
	{
		char quote = headerValue[valueStart];
		++valueStart;
		valueEnd = headerValue.find(quote, valueStart);
	}
	else
	{
		valueEnd = headerValue.find(';', valueStart);
	}

	if (valueEnd == std::string::npos)
		valueEnd = headerValue.size();
	if (valueEnd <= valueStart)
		return "";

	std::string extracted = trimAsciiEngine(headerValue.substr(valueStart, valueEnd - valueStart));
	if (isUnsafeUploadFilenameEngine(extracted))
		return "";
	return extracted;
}

static std::string defaultUploadFilenameEngine()
{
	std::ostringstream oss;
	oss << "upload_" << std::time(NULL) << "_" << std::rand() << ".bin";
	return oss.str();
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

static bool ensureDirectoryExistsEngine(const std::string& path)
{
	if (path.empty())
		return false;

	struct stat st;
	if (stat(path.c_str(), &st) == 0)
		return S_ISDIR(st.st_mode);

	if (mkdir(path.c_str(), 0755) == 0)
		return true;

	if (errno == EEXIST && stat(path.c_str(), &st) == 0)
		return S_ISDIR(st.st_mode);
	return false;
}

const LocationConfig* Engine::findBestLocation(const ServerConfig& serverConfig, const std::string& path) const
{
	const std::vector<LocationConfig>& locations = serverConfig.getLocations();
	const LocationConfig* best = NULL;
	size_t bestLen = 0;

	for (size_t i = 0; i < locations.size(); ++i)
	{
		const std::string& locPath = locations[i].getPath();
		if (path.find(locPath) != 0)
			continue;

		bool boundaryMatch = (locPath == "/"
			|| path.size() == locPath.size()
			|| path[locPath.size()] == '/');
		if (boundaryMatch && locPath.size() >= bestLen)
		{
			best = &locations[i];
			bestLen = locPath.size();
		}
	}
	return best;
}

bool Engine::isMethodAllowed(const std::string& method, const LocationConfig* location) const
{
	if (location == NULL)
		return true;

	const std::vector<std::string>& allowed = location->getAllowedMethods();
	if (allowed.empty())
		return true;

	for (size_t i = 0; i < allowed.size(); ++i)
	{
		if (allowed[i] == method)
			return true;
	}
	return false;
}

bool Engine::prepareConnection(Connection* conn, const char* buffer, ssize_t bytes, size_t& headerEnd)
{
	conn->appendToReadBuffer(buffer, bytes);
	conn->updateActivity();

	std::string& readBuffer = conn->getReadBuffer();
	headerEnd = readBuffer.find("\r\n\r\n");
	if (!enforceRequestSizeLimits(conn, headerEnd))
		return false;

	// update request state
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

// request line : 1) method, 2) path, 3) version, eg: GET / HTTP/1.1 \r\n
// header : 1) localhost, 2) content-length
// empty line
// body (optional)

void Engine::handleClientRequest(Connection* conn, const char* buffer, ssize_t bytes)
{
	size_t headerEnd = std::string::npos;
	if (!prepareConnection(conn, buffer, bytes, headerEnd))
		return;

	bool producedResponse = false;
	if (!processBufferedRequests(conn, producedResponse))
		return;

	//finalizeConnection
	if (producedResponse)
		conn->setState(Connection::WRITING);
	else
		conn->setState(Connection::READING);
}

std::string Engine::routeRequest(const HttpRequest& request, bool shouldClose, const ServerConfig& serverConfig)
{
	const LocationConfig* location = findBestLocation(serverConfig, request.getPath());
	if (location != NULL && location->hasReturnDirective())
		return buildRedirectResponse(location->getReturnStatus(), location->getReturnTarget(), shouldClose);
	if (!isMethodAllowed(request.getMethod(), location))
		return buildErrorResponse(405, shouldClose, &serverConfig, methodNotAllowedHeadersEngine(location));

	std::string root = serverConfig.getRoot();
	if (location != NULL && !location->getRoot().empty())
		root = location->getRoot();

	if (request.getMethod() == "GET")
	{
		if (request.getPath().find("..") != std::string::npos)
			return buildErrorResponse(403, shouldClose, &serverConfig);
		std::string path = FileHandler::resolvePath(request.getPath(), root, serverConfig.getIndex());
		struct stat s;
		if (stat(path.c_str(), &s) == 0 && S_ISDIR(s.st_mode))
		{
			std::string indexPath = path + "/" + serverConfig.getIndex();
			if (FileHandler::fileExists(indexPath))
				path = indexPath;
			else if (location != NULL && location->isAutoindex())
			{
				std::string listing = FileHandler::generateDirectoryListing(request.getPath(), path);
				if (listing.empty())
					return buildErrorResponse(500, shouldClose, &serverConfig);
				return buildStandardResponse(200, listing, "text/html", shouldClose);
			}
			else
				return buildErrorResponse(403, shouldClose, &serverConfig);
		}

		if (!FileHandler::fileExists(path))
			return buildErrorResponse(404, shouldClose, &serverConfig);
		std::string content = FileHandler::readFile(path);
		std::string mime = FileHandler::getMimeType(path);
		return buildStandardResponse(200, content, mime, shouldClose);
	}
	if (request.getMethod() == "POST")
		return handlePost(request, shouldClose, serverConfig, location);
	if (request.getMethod() == "DELETE")
		return handleDelete(request, shouldClose, serverConfig);
	return buildErrorResponse(405, shouldClose, &serverConfig, methodNotAllowedHeadersEngine(location));
}

std::string Engine::handlePost(const HttpRequest& request, bool shouldClose, const ServerConfig& serverConfig, const LocationConfig* location)
{
	const std::string& body = request.getBody();
	std::string uploadDir = serverConfig.getRoot();
	if (location != NULL && location->isUploadEnabled() && !location->getUploadPath().empty())
		uploadDir = location->getUploadPath();

	if (!ensureDirectoryExistsEngine(uploadDir))
		return buildErrorResponse(500, shouldClose, &serverConfig);

	std::string filename = extractUploadFilenameEngine(request);
	bool hadHeaderFilename = !filename.empty();
	if (filename.empty())
		filename = defaultUploadFilenameEngine();

	std::string uploadPath = uploadDir + "/" + filename;
	int fd = open(uploadPath.c_str(), O_CREAT | O_WRONLY | O_EXCL, 0644);
	if (fd < 0 && errno == EEXIST && hadHeaderFilename)
	{
		filename = defaultUploadFilenameEngine();
		uploadPath = uploadDir + "/" + filename;
		fd = open(uploadPath.c_str(), O_CREAT | O_WRONLY | O_EXCL, 0644);
	}
	if (fd < 0)
		return buildErrorResponse(500, shouldClose, &serverConfig);

	size_t total = 0;
	while (total < body.size())
	{
		ssize_t written = write(fd, body.data() + total, body.size() - total);
		if (written <= 0)
		{
			close(fd);
			return buildErrorResponse(500, shouldClose, &serverConfig);
		}
		total += written;
	}
	close(fd);
	return buildStandardResponse(201, "Upload OK", "text/plain", shouldClose);
}

std::string Engine::handleDelete(const HttpRequest& request, bool shouldClose, const ServerConfig& serverConfig)
{
	if (request.getPath().find("..") != std::string::npos)
		return buildErrorResponse(403, shouldClose, &serverConfig);

	const LocationConfig* location = findBestLocation(serverConfig, request.getPath());
	std::string root = serverConfig.getRoot();
	if (location != NULL && !location->getRoot().empty())
		root = location->getRoot();

	std::string path = FileHandler::resolvePath(request.getPath(), root, serverConfig.getIndex());
	if (!FileHandler::fileExists(path))
		return buildErrorResponse(404, shouldClose, &serverConfig);

	if (std::remove(path.c_str()) != 0)
		return buildErrorResponse(500, shouldClose, &serverConfig);

	return buildStandardResponse(204, "", "text/plain", shouldClose);
}
