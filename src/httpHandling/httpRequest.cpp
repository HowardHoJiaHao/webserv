/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   httpRequest.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: Ho Wai Keong <hwai_keo@student.42kl.edu    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/21 16:47:13 by hwai-keo          #+#    #+#             */
/*   Updated: 2026/04/05 11:35:39 by Ho Wai Keon      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "httpRequest.hpp"
#include <sstream>
#include <cstdlib>
#include <iostream>
#include <cctype>

static std::string toLowerAscii(const std::string& input)
{
	std::string lowered = input;
	for (size_t i = 0; i < lowered.size(); ++i)
		lowered[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lowered[i])));
	return lowered;
}

HttpRequest::HttpRequest()
	: _method(""),
	  _path(""),
	  _query(""),
	  _version(""),
	  _body(""),
	  _contentLength(0),
	  _hasContentLength(false)
{
}

const std::string& HttpRequest::getMethod() const
{
	return _method;
}

const std::string& HttpRequest::getPath() const
{
	return _path;
}

const std::string& HttpRequest::getQuery() const
{
	return _query;
}

const std::string& HttpRequest::getVersion() const
{
	return _version;
}

const std::string* HttpRequest::getHeader(const std::string& key) const
{
	std::map<std::string, std::string>::const_iterator it = _headers.find(toLowerAscii(key));
	if (it == _headers.end())
		return NULL;
	return &it->second;
}

const std::string& HttpRequest::getBody() const
{
	return _body;
}

bool HttpRequest::hasContentLength() const
{
	return _hasContentLength;
}

size_t HttpRequest::getContentLength() const
{
	return _contentLength;
}

bool HttpRequest::isComplete() const
{
	if (!_hasContentLength)
		return true;
	return _body.size() == _contentLength;
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
		if(value.empty())
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

void HttpRequest::parse(const std::string& rawRequest, size_t maxBodySize)
{
	reset();

	size_t headerEnd = rawRequest.find("\r\n\r\n");
	if (headerEnd == std::string::npos)
		throw std::runtime_error("Incomplete request");

	size_t lineEnd = rawRequest.find("\r\n");
	if (lineEnd == std::string::npos)
		throw std::runtime_error("Invalid request line");

	std::string requestLine = rawRequest.substr(0, lineEnd);
	parseRequestLine(requestLine);

	std::string headerSection = rawRequest.substr(0, headerEnd);
	parseHeaders(headerSection);
	validateHeaders(maxBodySize);
	extractBody(rawRequest, headerEnd);
}

bool HttpRequest::shouldCloseConnection() const
{
	std::map<std::string, std::string>::const_iterator it = _headers.find("connection");

	if (it != _headers.end())
	{
		const std::string value = toLowerAscii(it->second);

		if (value == "close")
			return true;
		if (value == "keep-alive")
			return false;
	}
	if (_version == "HTTP/1.1")
		return false;
	return true;
}

std::string HttpRequest::trim(const std::string& str)
{
	size_t start = 0;

	while (start < str.size() && std::isspace(str[start]))
		start++;
	size_t end = str.size();
	while (end > start && std::isspace(str[end - 1]))
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

std::string HttpRequest::getCookie(const std::string& key) const
{
	std::map<std::string, std::string>::const_iterator it = _cookies.find(key);
	if (it != _cookies.end())
		return it->second;
	return "";
}