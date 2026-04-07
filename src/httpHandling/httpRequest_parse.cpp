/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   httpRequest_parse.cpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hwai-keo <hwai-keo@student.42kl.edu.my>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/07 16:10:00 by hwai-keo          #+#    #+#             */
/*   Updated: 2026/04/07 15:50:02 by hwai-keo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "httpRequest.hpp"
#include <sstream>
#include <cstdlib>
#include <cctype>

//	====================	http parsing	======================

static std::string toLowerAscii(const std::string& input)
{
	std::string lowered = input;
	for (size_t i = 0; i < lowered.size(); ++i)
		lowered[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lowered[i])));
	return lowered;
}

void HttpRequest::reset()
{
	_method.clear();
	_path.clear();
	_query.clear();
	_version.clear();
	_body.clear();
	_cookies.clear();
	_headers.clear();
	_contentLength = 0;
	_hasContentLength = false;
}

void HttpRequest::parseRequestLine(const std::string& requestLine)
{
	std::istringstream iss(requestLine);
	if (!(iss >> _method >> _path >> _version))
		throw std::runtime_error("Malformed request line");

	size_t queryPos = _path.find('?');
	if (queryPos != std::string::npos)
	{
		if (queryPos + 1 < _path.size())
			_query = _path.substr(queryPos + 1);
		_path = _path.substr(0, queryPos);
	}

	if (_path.empty() || _path[0] != '/')
		throw std::runtime_error("invalid path");

	if (_version.find("HTTP/") != 0)
		throw std::runtime_error("invalid http version");

	std::string extra;
	if (iss >> extra)
		throw std::runtime_error("redundant token in request line");
}

void HttpRequest::parseHeaders(const std::string& headerSection)
{
	std::istringstream stream(headerSection);
	std::string line;
	bool firstLine = true;

	while (std::getline(stream, line))
	{
		if (!line.empty() && line[line.length() - 1] == '\r')
			line.erase(line.length() - 1);
		if (firstLine)
		{
			firstLine = false;
			continue;
		}
		if (line.empty())
			continue;

		size_t colonPos = line.find(':');
		if (colonPos == std::string::npos)
			throw std::runtime_error("malformed header");
		std::string key = toLowerAscii(line.substr(0, colonPos));
		std::string value = line.substr(colonPos + 1);

		if (key.empty())
			throw std::runtime_error("Malformed header: empty key");
		while (!value.empty() && (value[0] == ' ' || value[0] == '\t'))
			value.erase(0, 1);
		if (_headers.find(key) != _headers.end())
			throw std::runtime_error("Duplicate header");
		_headers[key] = value;
		if (key == "cookie")
			parseCookies(value);
	}
}

void HttpRequest::validateHeaders(size_t maxBodySize)
{
	if (_version == "HTTP/1.1")
	{
		std::map<std::string, std::string>::const_iterator hostIt = _headers.find("host");
		if (hostIt == _headers.end())
			throw std::runtime_error("Missing Host header");
		if (hostIt->second.empty())
			throw std::runtime_error("Invalid Host header");
	}

	std::map<std::string, std::string>::const_iterator it = _headers.find("content-length");
	if (it != _headers.end())
	{
		const std::string& value = it->second;
		if (value.empty())
			throw std::runtime_error("Empty Content-Length");
		for (size_t i = 0; i < value.length(); ++i)
		{
			if (!isdigit(static_cast<unsigned char>(value[i])))
				throw std::runtime_error("Invalid Content-Length");
		}
		std::istringstream iss(value);
		iss >> _contentLength;

		if (iss.fail())
			throw std::runtime_error("Invalid Content-Length");
		if (_contentLength > maxBodySize)
			throw std::runtime_error("Body too large");
		_hasContentLength = true;
	}
}

void HttpRequest::extractBody(const std::string& rawRequest, size_t headerEnd)
{
	size_t bodyStart = headerEnd + 4;
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
	validateHeaders(maxBodySize);
	extractBody(rawRequest, headerEnd);
}

// =============================	cookie parsing		====================

std::string HttpRequest::trim(const std::string& str)
{
	size_t start = 0;

	while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start])))
		start++;
	size_t end = str.size();
	while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1])))
		end--;
	return str.substr(start, end - start);
}

void HttpRequest::parseCookies(const std::string& cookieHeader)
{
	std::stringstream ss(cookieHeader);
	std::string pair;

	while (std::getline(ss, pair, ';'))
	{
		size_t eqPos = pair.find('=');
		if (eqPos == std::string::npos)
			continue;
		std::string key = pair.substr(0, eqPos);
		std::string value = pair.substr(eqPos + 1);

		key = trim(key);
		value = trim(value);

		_cookies[key] = value;
	}
}