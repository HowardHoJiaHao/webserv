#include "engine.hpp"
#include "FileHandler.hpp"

#include <sstream>

static std::string reasonPhraseForStatusCodeEngine(int code)
{
	switch (code)
	{
		case 200: return "OK";
		case 201: return "Created";
		case 204: return "No Content";
		case 301: return "Moved Permanently";
		case 302: return "Found";
		case 303: return "See Other";
		case 307: return "Temporary Redirect";
		case 308: return "Permanent Redirect";
		case 400: return "Bad Request";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 408: return "Request Timeout";
		case 413: return "Payload Too Large";
		case 500: return "Internal Server Error";
		case 504: return "Gateway Timeout";
		default: return "Internal Server Error";
	}
}

std::string Engine::buildStandardResponse(int code, const std::string& body, const std::string& contentType, bool shouldClose)
{
	std::ostringstream status;
	status << code << " " << reasonPhraseForStatusCodeEngine(code);
	return buildResponse(status.str(), body, contentType, shouldClose, std::vector<std::string>());
}

// example of response
// HTTP/1.1 404 Not Found
// Content-Type: text/plain
// Content-Length: 0
// Connection: keep-alive

// HTTP/1.1 301 Moved Permanently
// Content-Type: text/plain
// Content-Length: 0
// Location: https://example.com
// Connection: keep-alive

// HTTP/1.1 405 Method Not Allowed
// Content-Type: text/plain
// Content-Length: 0
// Allow: GET
// Connection: keep-alive

// location /42kl
// {
//     return 301 https://42kl.edu.my/; <- return target and return code is here
// }
std::string Engine::buildRedirectResponse(int code, const std::string& target, bool shouldClose)
{
	// parser should handle this instead, which throw error
	// enable me a limited possible response for later reasonPhraseForStatusCodeEngine()
	if (code != 301 && code != 302 && code != 303 && code != 307 && code != 308)
		code = 302;

	std::vector<std::string> extraheaders;
	extraheaders.push_back("Location: " + target);

	std::ostringstream status;
	status << code << " " << reasonPhraseForStatusCodeEngine(code);
	return buildResponse(status.str(), "", "text/plain", shouldClose, extraheaders);
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

std::string Engine::appendHeaderToResponse(const std::string& response, const std::string& headerLine) const
{
	if (headerLine.empty())
		return response;

	// If not found, it returns unchanged (it assumes the input is not a valid full HTTP response format).
	size_t headerEnd = response.find("\r\n\r\n");
	if (headerEnd == std::string::npos)
		return response;

	// original:
	// HTTP status + existing headers + blank line + body

	// after function:
	// HTTP status + existing headers + new header + blank line + body
	std::string withHeader = response.substr(0, headerEnd);
	withHeader += "\r\n";
	withHeader += headerLine;
	withHeader += response.substr(headerEnd);
	return withHeader;
}

std::string Engine::buildErrorResponse(int code, bool shouldClose, const ServerConfig* serverConfig)
{
	return buildErrorResponse(code, shouldClose, serverConfig, std::vector<std::string>());
}

std::string Engine::buildErrorResponse(int code, bool shouldClose, const ServerConfig* serverConfig, const std::vector<std::string>& extraHeaders)
{
	const std::string reason = reasonPhraseForStatusCodeEngine(code);

	if (serverConfig != NULL)
	{
		// find if the error page exist
		const std::string* pagePath = serverConfig->getErrorPage(code);
		if (pagePath != NULL)
		{
			std::string fullPath = serverConfig->getRoot() + *pagePath;
			if (FileHandler::fileExists(fullPath))
			{
				std::string body = FileHandler::readFile(fullPath);
				std::ostringstream status;
				status << code << " " << reason;
				return buildResponse(status.str(), body, "text/html", shouldClose, extraHeaders);
			}
		}
	}

	std::ostringstream status;
	status << code << " " << reason;
	return buildResponse(status.str(), reason, "text/plain", shouldClose, extraHeaders);
}
