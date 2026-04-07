#include "engine.hpp"
#include "engine_string_utils.hpp"
#include "FileHandler.hpp"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cerrno>

// ==========================		routing util		=======================

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

// =====================		routing		==========================

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

std::string Engine::routeRequest(const HttpRequest& request, bool shouldClose, const ServerConfig& serverConfig)
{
	const LocationConfig* location = findBestLocation(serverConfig, request.getPath());
	if (location != NULL && location->hasReturnDirective())
		return buildRedirectResponse(location->getReturnStatus(), location->getReturnTarget(), shouldClose);
	if (!isMethodAllowed(request.getMethod(), location))
		return buildErrorResponse(405, shouldClose, &serverConfig, methodNotAllowedHeadersEngine(location));

	if (request.getMethod() == "GET")
		return handleGet(request, shouldClose, serverConfig, location);
	if (request.getMethod() == "POST")
		return handlePost(request, shouldClose, serverConfig, location);
	if (request.getMethod() == "DELETE")
		return handleDelete(request, shouldClose, serverConfig);
	return buildErrorResponse(405, shouldClose, &serverConfig, methodNotAllowedHeadersEngine(location));
}

std::string Engine::handleGet(const HttpRequest& request, bool shouldClose, const ServerConfig& serverConfig, const LocationConfig* location)
{
	if (request.getPath().find("..") != std::string::npos)
		return buildErrorResponse(403, shouldClose, &serverConfig);

	std::string root = serverConfig.getRoot();
	if (location != NULL && !location->getRoot().empty())
		root = location->getRoot();

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
