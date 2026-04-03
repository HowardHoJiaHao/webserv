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

// request line : 1) method, 2) path, 3) version, eg: GET / HTTP/1.1 \r\n
// header : 1) localhost, 2) content-length
// empty line
// body (optional)

void Engine::handleClientRequest(Connection* conn, const char* buffer, ssize_t bytes)
{
	conn->appendToReadBuffer(buffer, bytes);
	conn->updateActivity();

	const size_t maxHeaderSize = 8192;

	std::string& readBuffer = conn->getReadBuffer();
	size_t headerEnd = readBuffer.find("\r\n\r\n");
	const ServerConfig* defaultServerForLimit = findServerConfig(
		_clientListenEndpoints[conn->getFd()].first,
		_clientListenEndpoints[conn->getFd()].second
	);
	size_t maxBodySizeLimit = MAX_REQUEST_SIZE;
	if (defaultServerForLimit != NULL)
		maxBodySizeLimit = defaultServerForLimit->getMaxBodySize();
	size_t maxBufferedRequestSize = maxBodySizeLimit + maxHeaderSize;
	if (headerEnd == std::string::npos)
	{
		if(readBuffer.size() > maxHeaderSize)
		{
			conn->setShouldClose(true);
			conn->getReadBuffer().clear();
			conn->getWriteBuffer() = buildResponse
			(
				"413 Payload Too Large",
				"Header Too Large",
				"text/plain",
				conn->shouldClose(),
				std::vector<std::string>()
			);
			conn->setState(Connection::WRITING);
			return;
		}
	}


	if (conn->getReadBuffer().size() > maxBufferedRequestSize)
	{
		conn->setShouldClose(true);
		conn->getReadBuffer().clear();
		conn->getWriteBuffer() = buildResponse
		(
			"413 Payload Too Large",
			"Payload Too Large",
			"text/plain",
			conn->shouldClose(),
			std::vector<std::string>()
		);
		conn->setState(Connection::WRITING);
		return;
	}

	std::string rawRequest;
	bool producedResponse = false;
	if (headerEnd == std::string::npos)
		conn->setRequestState(Connection::READING_HEADERS);
	else
		conn->setRequestState(Connection::READING_BODY);

	while (true)
	{
		bool malformed = false;
		if (!extractRequest(readBuffer, rawRequest, &malformed))
		{
			if (malformed)
			{
				conn->setShouldClose(true);
				conn->getWriteBuffer() = build400Response(conn->shouldClose(), NULL);
				conn->setState(Connection::WRITING);
				return;
			}
			if (readBuffer.find("\r\n\r\n") == std::string::npos)
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
				conn->getWriteBuffer() = buildErrorResponse(413, "Payload Too Large", conn->shouldClose(), defaultServer);
			else
				conn->getWriteBuffer() = build400Response(conn->shouldClose(), defaultServer);
			conn->setState(Connection::WRITING);
			return;
		}

		const ServerConfig* serverConfig = conn->getServerConfig();
		if (serverConfig != NULL && request.hasContentLength() && request.getContentLength() > serverConfig->getMaxBodySize())
		{
			conn->setShouldClose(true);
			conn->getWriteBuffer() = buildErrorResponse(413, "Payload Too Large", conn->shouldClose(), serverConfig);
			conn->setState(Connection::WRITING);
			return;
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
			conn->getWriteBuffer() = buildErrorResponse(500, "Internal Server Error", conn->shouldClose(), NULL);
			conn->setState(Connection::WRITING);
			return;
		}

		const LocationConfig* matchedLocation = findBestLocation(*effectiveServer, request.getPath());
		if (!isMethodAllowed(request.getMethod(), matchedLocation))
		{
			conn->getWriteBuffer() = build405Response(conn->shouldClose(), effectiveServer, matchedLocation);
			conn->setState(Connection::WRITING);
			return;
		}

		if (matchedLocation != NULL && isCgiRequestForLocation(request.getPath(), *matchedLocation))
		{
			if (!launchCGI(conn, request, *effectiveServer, conn->shouldClose()))
				conn->setState(Connection::WRITING);
			return;
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

	if (producedResponse)
		conn->setState(Connection::WRITING);
	else
		conn->setState(Connection::READING);
}

std::string Engine::routeRequest(const HttpRequest& request, bool shouldClose, const ServerConfig& serverConfig)
{
	const LocationConfig* location = findBestLocation(serverConfig, request.getPath());
	if (location != NULL && location->hasReturnDirective())
	{
		std::vector<std::string> headers;
		headers.push_back("Location: " + location->getReturnTarget());
		std::string status = "302 Found";
		if (location->getReturnStatus() == 301)
			status = "301 Moved Permanently";
		else if (location->getReturnStatus() == 303)
			status = "303 See Other";
		else if (location->getReturnStatus() == 307)
			status = "307 Temporary Redirect";
		else if (location->getReturnStatus() == 308)
			status = "308 Permanent Redirect";
		return buildResponse(status, "", "text/plain", shouldClose, headers);
	}
	if (!isMethodAllowed(request.getMethod(), location))
		return build405Response(shouldClose, &serverConfig, location);

	std::string root = serverConfig.getRoot();
	if (location != NULL && !location->getRoot().empty())
		root = location->getRoot();

	if (request.getMethod() == "GET")
	{
		if (request.getPath().find("..") != std::string::npos)
			return buildErrorResponse(403, "Forbidden", shouldClose, &serverConfig);
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
					return buildErrorResponse(500, "Internal Server Error", shouldClose, &serverConfig);
				return buildResponse("200 OK", listing, "text/html", shouldClose, std::vector<std::string>());
			}
			else
				return buildErrorResponse(403, "Forbidden", shouldClose, &serverConfig);
		}

		if (!FileHandler::fileExists(path))
			return build404Response(shouldClose, &serverConfig);
		std::string content = FileHandler::readFile(path);
		std::string mime = FileHandler::getMimeType(path);
		return buildResponse("200 OK", content, mime, shouldClose, std::vector<std::string>());
	}
	if (request.getMethod() == "POST")
		return handlePost(request, shouldClose, serverConfig, location);
	if (request.getMethod() == "DELETE")
		return handleDelete(request, shouldClose, serverConfig);
	return build405Response(shouldClose, &serverConfig, location);
}

std::string Engine::handlePost(const HttpRequest& request, bool shouldClose, const ServerConfig& serverConfig, const LocationConfig* location)
{
	const std::string& body = request.getBody();
	std::string uploadDir = serverConfig.getRoot();
	if (location != NULL && location->isUploadEnabled() && !location->getUploadPath().empty())
		uploadDir = location->getUploadPath();

	if (!ensureDirectoryExistsEngine(uploadDir))
		return buildErrorResponse(500, "Internal Server Error", shouldClose, &serverConfig);

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
		return buildErrorResponse(500, "Internal Server Error", shouldClose, &serverConfig);

	size_t total = 0;
	while (total < body.size())
	{
		ssize_t written = write(fd, body.data() + total, body.size() - total);
		if (written <= 0)
		{
			close(fd);
			return buildErrorResponse(500, "Internal Server Error", shouldClose, &serverConfig);
		}
		total += written;
	}
	close(fd);
	return buildResponse("201 Created", "Upload OK", "text/plain", shouldClose, std::vector<std::string>());
}

std::string Engine::handleDelete(const HttpRequest& request, bool shouldClose, const ServerConfig& serverConfig)
{
	if (request.getPath().find("..") != std::string::npos)
		return buildErrorResponse(403, "Forbidden", shouldClose, &serverConfig);

	const LocationConfig* location = findBestLocation(serverConfig, request.getPath());
	std::string root = serverConfig.getRoot();
	if (location != NULL && !location->getRoot().empty())
		root = location->getRoot();

	std::string path = FileHandler::resolvePath(request.getPath(), root, serverConfig.getIndex());
	if (!FileHandler::fileExists(path))
		return build404Response(shouldClose, &serverConfig);

	if (std::remove(path.c_str()) != 0)
		return buildErrorResponse(500, "Internal Server Error", shouldClose, &serverConfig);

	return buildResponse("204 No Content", "", "text/plain", shouldClose, std::vector<std::string>());
}

std::string Engine::buildResponse
(
	const std::string& status,
	const std::string& body,
	const std::string& contentType,
	bool shouldClose,
	const std::vector<std::string>& extraHeaders
)
{
	std::stringstream ss;
	ss << "HTTP/1.1 " << status << "\r\n";
	ss << "Content-Length: " << body.size() << "\r\n";
	ss << "Content-Type: " << contentType << "\r\n";

	for (size_t i = 0; i < extraHeaders.size(); i++)
	{
		ss << extraHeaders[i] << "\r\n";
	}
	if (shouldClose)
		ss << "Connection: close\r\n";
	else
		ss << "Connection: keep-alive\r\n";
	ss << "\r\n";
	ss << body;
	return ss.str();
}

std::string Engine::buildErrorResponse(int code, const std::string& defaultMsg, bool shouldClose, const ServerConfig* serverConfig)
{
	if (serverConfig != NULL)
	{
		const std::string* pagePath = serverConfig->getErrorPage(code);
		if (pagePath != NULL)
		{
			std::string fullPath = serverConfig->getRoot() + *pagePath;
			if (FileHandler::fileExists(fullPath))
			{
				std::string body = FileHandler::readFile(fullPath);
				std::ostringstream status;
				status << code << " " << defaultMsg;
				return buildResponse(status.str(), body, "text/html", shouldClose, std::vector<std::string>());
			}
		}
	}

	std::ostringstream status;
	status << code << " " << defaultMsg;
	return buildResponse(status.str(), defaultMsg, "text/plain", shouldClose, std::vector<std::string>());
}

std::string Engine::build405Response(bool shouldClose, const ServerConfig* serverConfig, const LocationConfig* location)
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

	if (serverConfig != NULL)
	{
		const std::string* pagePath = serverConfig->getErrorPage(405);
		if (pagePath != NULL)
		{
			std::string fullPath = serverConfig->getRoot() + *pagePath;
			if (FileHandler::fileExists(fullPath))
			{
				std::string body = FileHandler::readFile(fullPath);
				return buildResponse("405 Method Not Allowed", body, "text/html", shouldClose, headers);
			}
		}
	}

	return buildResponse("405 Method Not Allowed", "Method Not Allowed", "text/plain", shouldClose, headers);
}

std::string Engine::build404Response(bool shouldClose, const ServerConfig* serverConfig)
{
	return buildErrorResponse(404, "Not Found", shouldClose, serverConfig);
}

std::string Engine::build400Response(bool shouldClose, const ServerConfig* serverConfig)
{
	return buildErrorResponse(400, "Bad Request", shouldClose, serverConfig);
}
