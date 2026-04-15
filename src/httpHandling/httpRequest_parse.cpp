/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   httpRequest_parse.cpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hwai-keo <hwai-keo@student.42kl.edu.my>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/07 16:10:00 by hwai-keo          #+#    #+#             */
/*   Updated: 2026/04/15 18:06:26 by hwai-keo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "httpRequest.hpp"
#include "requestValidator.hpp"
#include "engine_string_utils.hpp"
#include <sstream>
#include <cctype>

//	====================	http parsing	======================

static bool hasChunkedTransferEncoding(const std::map<std::string, std::string>& headers)
{
	std::map<std::string, std::string>::const_iterator it = headers.find("transfer-encoding");
	if (it == headers.end())
		return false;
	return hasChunkedTransferEncodingValue(it->second);
}

void HttpRequest::reset()
{
	_method.clear();
	_path.clear();
	_query.clear();
	_version.clear();
	_body.clear();
	_headers.clear();
	_cookies.clear();
	_contentLength = 0;
	_hasContentLength = false;
}

// requestLine: GET /index.html HTTP/1.1\r\n
void HttpRequest::parseRequestLine(const std::string& requestLine)
{
	std::istringstream iss(requestLine);
	// split by white space
	if (!(iss >> _method >> _path >> _version))
		throw std::runtime_error("Malformed request line");

	//example, this is for cgi
	//http://127.0.0.1:8080/cgi-bin/delete.py?format=json
	size_t queryPos = _path.find('?');
	if (queryPos != std::string::npos)
	{
		if (queryPos + 1 < _path.size())
			_query = _path.substr(queryPos + 1);
		_path = _path.substr(0, queryPos);
	}

	if (_path.empty() || _path[0] != '/')
		throw std::runtime_error("invalid path");

	// must start with http
	if (_version.find("HTTP/") != 0)
		throw std::runtime_error("invalid http version");

	// the rest
	std::string extra;
	if (iss >> extra)
		throw std::runtime_error("redundant token in request line");
}

// GET /index.html HTTP/1.1\r\n <==this will be skip 
// Host: example.com\r\n
// User-Agent: Mozilla/5.0\r\n
// Accept: */*\r\n
// \r\n

// expected output
// {
//		"host"			→ "example.com",
//		"user-agent"	→ "Mozilla/5.0",
//		"accept"		→ "*/*"
// }
void HttpRequest::parseHeaders(const std::string& headerSection)
{
	std::istringstream stream(headerSection);
	std::string line;

	//skip first line, which is requestline
	std::getline(stream, line);
	while (std::getline(stream, line))
	{
		if (!line.empty() && line[line.length() - 1] == '\r')
			line.erase(line.length() - 1);
		if (line.empty())
			continue;
		size_t colonPos = line.find(':');
		if (colonPos == std::string::npos)
			throw std::runtime_error("malformed header");
		std::string key = toLowerAsciiEngine(line.substr(0, colonPos));
		std::string value = line.substr(colonPos + 1);
		if (key.empty())
			throw std::runtime_error("Malformed header: empty key");
		// remove leading white space
		while (!value.empty() && (value[0] == ' ' || value[0] == '\t'))
			value.erase(0, 1);
		// the current key that just created, check with the header pool
		if (_headers.find(key) != _headers.end())
			throw std::runtime_error("Duplicate header");
		_headers[key] = value;
	}
}

// _headers = {
//		"host"			: "example.com",
//		"user-agent"	: "Mozilla/5.0",
//		"accept"		: "*/*",
//		"cookie"		: "sessionId=abc123; theme=dark; lang=en"
// }
void HttpRequest::parseCookies()
{
	// find cookie in header
	std::map<std::string, std::string>::const_iterator cookieHeader = _headers.find("cookie");
	if (cookieHeader == _headers.end())
		return;

	// cookieHeader->second is the value of cookie
	// "sessionId=abc123; theme=dark; lang=en" <= raw cookie
	const std::string& rawCookies = cookieHeader->second;
	size_t start = 0;
	while (start <= rawCookies.size())
	{
		size_t delimiter = rawCookies.find(';', start);
		std::string token;
		// if no ';', then the token is the rest of the string
		if (delimiter == std::string::npos)
			token = rawCookies.substr(start);
		// else taking anything before ';'
		else
			token = rawCookies.substr(start, delimiter - start);

		// remove leading and trailing whitespace
		token = trimAsciiEngine(token);
		if (!token.empty())
		{
			// if token exist, split by '=' and store the left and right in _cookies
			size_t equalsPos = token.find('=');
			if (equalsPos != std::string::npos)
			{
				std::string cookieKey = toLowerAsciiEngine(trimAsciiEngine(token.substr(0, equalsPos)));
				std::string cookieValue = trimAsciiEngine(token.substr(equalsPos + 1));
				// if cookieKey is not empty and not exist in _cookies, then add it
				if (!cookieKey.empty() && _cookies.find(cookieKey) == _cookies.end())
					_cookies[cookieKey] = cookieValue;
			}
		}

		// means there is the last token without end with ';'
		if (delimiter == std::string::npos)
			break;
		// skip delimiter
		start = delimiter + 1;
	}
}

// _headers =
// {
//		"host"				→ "example.com",
//		"user-agent"		→ "Mozilla/5.0",
//		"content-length"	→ "11"
// }
void HttpRequest::validateHeaders(size_t maxBodySize)
{
	if (_version == "HTTP/1.1")
	{
		std::map<std::string, std::string>::const_iterator hostIt = _headers.find("host");

		// if host key not found or value in host is empty
		if (hostIt == _headers.end())
			throw std::runtime_error("Missing Host header");
		if (hostIt->second.empty())
			throw std::runtime_error("Invalid Host header");
	}

	bool hasChunkedBody = hasChunkedTransferEncoding(_headers);
	// if chunked body and content-length header exist, throw error, they should not coexist
	if (hasChunkedBody && _headers.find("content-length") != _headers.end())
		throw std::runtime_error("Invalid Content-Length");

	// find content length from header: "content-length"	→ "11"
	std::map<std::string, std::string>::const_iterator it = _headers.find("content-length");
	if (it != _headers.end())
	{
		// get the value of content-length, eg : 11
		const std::string& value = it->second;
		if (value.empty())
			throw std::runtime_error("Empty Content-Length");
		// check if all characters are digits
		for (size_t i = 0; i < value.length(); ++i)
		{
			if (!isdigit(static_cast<unsigned char>(value[i])))
				throw std::runtime_error("Invalid Content-Length");
		}
		// std::string -> size_t
		std::istringstream iss(value);
		iss >> _contentLength;
		// empty string, too big, will fail
		if (iss.fail())
			throw std::runtime_error("Invalid Content-Length");
		if (_contentLength > maxBodySize)
			throw std::runtime_error("Body too large");
		_hasContentLength = true;
	}
}

// "POST /submit HTTP/1.1\r\n" <= rawRequest
// "Host: example.com\r\n"
// "Content-Length: 11\r\n"
// "\r\n"		<== headerEnd
// "Hello World"

// check whether it is chunked encoding or content length
// then extract the body
void HttpRequest::extractBody(const std::string& rawRequest, size_t headerEnd, size_t maxBodySize)
{
	// reach end of string
	size_t bodyStart = headerEnd + 4;
	if (bodyStart > rawRequest.size())
		throw std::runtime_error("Incomplete request");

	// if chunked body header exist
	if (hasChunkedTransferEncoding(_headers))
	{
		bool bodyTooLarge = false;
		// check if large body, and update bodyTooLarge flag
		if (!decodeChunkedBodyForRequest(rawRequest.substr(bodyStart), maxBodySize, _body, &bodyTooLarge))
		{
			if (bodyTooLarge)
				throw std::runtime_error("Body too large");
			throw std::runtime_error("Malformed chunked body");
		}
		return;
	}

	// if content-length header exist
	if (_hasContentLength)
	{
		if (bodyStart + _contentLength > rawRequest.size())
			throw std::runtime_error("Body shorter than Content-Length");
		_body = rawRequest.substr(bodyStart, _contentLength);
	}
	else
	{
		_body.clear();
	}
}

// raw Request ->
// GET /index.html HTTP/1.1\r\n
// Host: example.com\r\n
// User-Agent: Mozilla/5.0\r\n
// Accept: */*\r\n
// \r\n
void HttpRequest::parse(const std::string& rawRequest, size_t maxBodySize)
{
	// clear previous request data
	reset();

	size_t headerEnd = rawRequest.find("\r\n\r\n");
	// start unwinding to caller, reach catch block, build response
	if (headerEnd == std::string::npos)
		throw std::runtime_error("Incomplete request");

	size_t lineEnd = rawRequest.find("\r\n");
	if (lineEnd == std::string::npos)
		throw std::runtime_error("Invalid request line");

	// GET /index.html HTTP/1.1
	std::string requestLine = rawRequest.substr(0, lineEnd);
	parseRequestLine(requestLine);

	std::string headerSection = rawRequest.substr(0, headerEnd);
	parseHeaders(headerSection);
	parseCookies();
	validateHeaders(maxBodySize);
	extractBody(rawRequest, headerEnd, maxBodySize);
}

