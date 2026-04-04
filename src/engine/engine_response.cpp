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

std::string Engine::buildRedirectResponse(int code, const std::string& target, bool shouldClose)
{
	if (code != 301 && code != 302 && code != 303 && code != 307 && code != 308)
		code = 302;

	std::vector<std::string> headers;
	headers.push_back("Location: " + target);

	std::ostringstream status;
	status << code << " " << reasonPhraseForStatusCodeEngine(code);
	return buildResponse(status.str(), "", "text/plain", shouldClose, headers);
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

std::string Engine::buildErrorResponse(int code, bool shouldClose, const ServerConfig* serverConfig)
{
	return buildErrorResponse(code, shouldClose, serverConfig, std::vector<std::string>());
}

std::string Engine::buildErrorResponse(int code, bool shouldClose, const ServerConfig* serverConfig, const std::vector<std::string>& extraHeaders)
{
	const std::string reason = reasonPhraseForStatusCodeEngine(code);

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
				status << code << " " << reason;
				return buildResponse(status.str(), body, "text/html", shouldClose, extraHeaders);
			}
		}
	}

	std::ostringstream status;
	status << code << " " << reason;
	return buildResponse(status.str(), reason, "text/plain", shouldClose, extraHeaders);
}
