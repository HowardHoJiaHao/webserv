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

bool Engine::hasPathTraversal(const std::string& path) const
{
	// path is const, not mutable
	std::string newPath = path; 
	for (size_t i = 0; i < newPath.size(); ++i)
	{
		newPath[i] = std::tolower(static_cast<unsigned char>(newPath[i]));
	}
	// hex value of .
	if (newPath.find("%2e%2e") != std::string::npos)
		return true;
	if (newPath.find("..") != std::string::npos)
		return true;
	return false;
}

// it try to access the root from the location first, if not found, then go to server root
std::string Engine::resolveLocationRoot(const ServerConfig& serverConfig, const LocationConfig* location) const
{
	if (location != NULL && !location->getRoot().empty())
		return location->getRoot();
	return serverConfig.getRoot();
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

// allowed : depends on location pointers points to which location
// allowed =
// [
//		"GET",
//		"POST",
//		"DELETE"
// ]
bool Engine::isMethodAllowed(const std::string& method, const LocationConfig* location) const
{
	
	if (location == NULL)
		return true;

	const std::vector<std::string>& allowed = location->getAllowedMethods();
	if (allowed.empty())
		return false;

	for (size_t i = 0; i < allowed.size(); ++i)
	{
		if (allowed[i] == method)
			return true;
	}
	return false;
}

std::vector<std::string> Engine::methodNotAllowedHeaders(const LocationConfig* location) const
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
	if (stat(path.c_str(), &st) != 0)
		return false;
	return S_ISDIR(st.st_mode);
}
// requestPath is from client: GET /index.html HTTP/1.1\r\n, which is the second part of the request line
//
// location with root: use custom root, modify the request path
// location without root: use default root, keep the request path as is
//
// request, strip off the first part of the path(location path), then append the root path at front, depends on location has root or not
std::string Engine::mapRequestPathForLocationRoot(const std::string& requestPath, const LocationConfig* location) const
{
	// also when the location doesnt have this root field, return request path
	if (location == NULL || location->getRoot().empty())
		return requestPath;

	// if location path is / only, then it will be incorrect to strip, so just return request path
	const std::string& locationPath = location->getPath();
	if (locationPath.empty() || locationPath == "/")
		return requestPath;

	// nothing to strip off if both are the same
	if (requestPath == locationPath)
		return "/";

	//1. request path is longer than location path eg, request path: /haha/form.html, location path: /haha
	//2. request path starts with location path, taking the request path to compare to location path
	//3. request path has a slash after the location path
	if (requestPath.size() > locationPath.size()
		&& requestPath.compare(0, locationPath.size(), locationPath) == 0
		&& requestPath[locationPath.size()] == '/')
	{
		return requestPath.substr(locationPath.size());
	}

	return requestPath;
}

static std::string resolveIndexForRequestEngine(const ServerConfig& serverConfig, const LocationConfig* location)
{
	if (location != NULL && !location->getIndex().empty())
		return location->getIndex();
	return serverConfig.getIndex();
}

// has return is
// location /42kl
// {
// 	return 301 https://42kl.edu.my/;
// }
std::string Engine::routeRequest(const HttpRequest& request, bool shouldClose, const ServerConfig& serverConfig, const LocationConfig* location)
{
	if (location != NULL && location->hasReturnDirective())
		return buildRedirectResponse(location->getReturnStatus(), location->getReturnTarget(), shouldClose);

	if (request.getMethod() == "GET")
		return handleGet(request, shouldClose, serverConfig, location);
	if (request.getMethod() == "POST")
		return handlePost(request, shouldClose, serverConfig, location);
	if (request.getMethod() == "DELETE")
		return handleDelete(request, shouldClose, serverConfig, location);
	return buildErrorResponse(405, shouldClose, &serverConfig, methodNotAllowedHeaders(location));
}

// http request: /haha/form.html -> location match: /haha -> root: ./www1 -> mappedPath: /form.html -> Finalpath: ./www1/form.html
std::string Engine::handleGet(const HttpRequest& request, bool shouldClose, const ServerConfig& serverConfig, const LocationConfig* location)
{
	if (hasPathTraversal(request.getPath()))
		return buildErrorResponse(403, shouldClose, &serverConfig);

	//preparing path:- it is where my webserver trying to get the file and send to client, root and indexName from config, mappedPath from request
	std::string root = resolveLocationRoot(serverConfig, location);
	std::string indexName = resolveIndexForRequestEngine(serverConfig, location);
	std::string mappedPath = mapRequestPathForLocationRoot(request.getPath(), location);
	std::string path = FileHandler::resolvePath(mappedPath, root, indexName);

	//create a variable to store the file status(telling is file/directory, file size, permission, timestamp)
	struct stat s;
	// check what is the path, does the path exist on disk and is it a directory
	if (stat(path.c_str(), &s) == 0 && S_ISDIR(s.st_mode))
	{
		std::string indexPath = path + "/" + indexName;
		// check the indexPath file is exist
		if (FileHandler::fileExists(indexPath))
			path = indexPath;
		// the directory exist but no index file and autoindex is on
		// no index file, but autoindex is enabled
		else if (location != NULL && location->isAutoindex())
		{
			//show directory listing
			std::string listing = FileHandler::generateDirectoryListing(request.getPath(), path);
			// failed to open directory, permission denied, error reading files
			if (listing.empty())
				return buildErrorResponse(500, shouldClose, &serverConfig);
			return buildStandardResponse(200, listing, "text/html", shouldClose);
		}
		// forbidden
		else
			return buildErrorResponse(403, shouldClose, &serverConfig);
	}
	// file does not exist
	if (!FileHandler::fileExists(path))
		return buildErrorResponse(404, shouldClose, &serverConfig);
	
	// read file content
	std::string fileContent = FileHandler::readFile(path);
	std::string multipurposeInternetMailExtensions = FileHandler::getMimeType(path);
	return buildStandardResponse(200, fileContent, multipurposeInternetMailExtensions, shouldClose);
}

// my post request config
// location /Upload
//{
//		methods POST;
//		upload_enable on;
//		upload_path ./uploads;
//		autoindex off;
// }
//
// this is what client send to server
//
// POST /Upload HTTP/1.1
// Host: 127.0.0.1:8080
// Content-Length: 11
//
// Content-Disposition: form-data; name="file"; filename="hello.txt" <- this one to tell how i will name the file
// Content-Type: text/plain
//
// hello world
std::string Engine::handlePost(const HttpRequest& request, bool shouldClose, const ServerConfig& serverConfig, const LocationConfig* location)
{
	// if upload is not allowed, return 403
	if (location == NULL || !location->isUploadEnabled())
		return buildErrorResponse(403, shouldClose, &serverConfig);
	// if no upload directory is specified, return 500
	if (location->getUploadPath().empty())
		return buildErrorResponse(500, shouldClose, &serverConfig);

	// get the data send by client(file content), store to upload path
	const std::string& body = request.getBody();
	std::string uploadDir = location->getUploadPath();

	// ensure the upload folder exist
	if (!ensureDirectoryExistsEngine(uploadDir))
		return buildErrorResponse(500, shouldClose, &serverConfig);

	// try get filename from request(content-disposition), if client didnt provide name, generate one	
	std::string filename = extractUploadFilenameEngine(request);
	bool hadHeaderFilename = !filename.empty();
	if (filename.empty())
		filename = defaultUploadFilenameEngine();

	//combine directory + file name, eg: /uploads/hello.txt
	std::string uploadPath = uploadDir + "/" + filename;
	// create new file only, failed if it exists
	int fd = open(uploadPath.c_str(), O_CREAT | O_WRONLY | O_EXCL, 0644);
	// if file exist and name come from client
	if (fd < 0 && errno == EEXIST && hadHeaderFilename)
	{
		// if file exist and name come from client, generate new default name
		filename = defaultUploadFilenameEngine();
		uploadPath = uploadDir + "/" + filename;
		fd = open(uploadPath.c_str(), O_CREAT | O_WRONLY | O_EXCL, 0644);
	}
	// if still failed, return 500
	if (fd < 0)
		return buildErrorResponse(500, shouldClose, &serverConfig);

	//write entire request body to file
	size_t total = 0;
	while (total < body.size())
	{
		//write chunk to disk, body.data() is actually works like pointer
		ssize_t written = write(fd, body.data() + total, body.size() - total);
		// if write failed, close file and return 500
		if (written <= 0)
		{
			close(fd);
			return buildErrorResponse(500, shouldClose, &serverConfig);
		}
		total += written;
	}
	close(fd);
	//success response
	return buildStandardResponse(201, "Upload OK", "text/plain", shouldClose);
}

std::string Engine::handleDelete(const HttpRequest& request, bool shouldClose, const ServerConfig& serverConfig, const LocationConfig* location)
{
	if (hasPathTraversal(request.getPath()))
		return buildErrorResponse(403, shouldClose, &serverConfig);

	std::string root = resolveLocationRoot(serverConfig, location);
	std::string mappedPath = mapRequestPathForLocationRoot(request.getPath(), location);
	std::string path = FileHandler::resolvePath(mappedPath, root, serverConfig.getIndex());
	if (!FileHandler::fileExists(path))
		return buildErrorResponse(404, shouldClose, &serverConfig);

	if (std::remove(path.c_str()) != 0)
		return buildErrorResponse(500, shouldClose, &serverConfig);

	return buildStandardResponse(204, "", "text/plain", shouldClose);
}
