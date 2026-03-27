/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   httpRequest.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hwai-keo <hwai-keo@student.42kl.edu.my>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/21 16:47:13 by hwai-keo          #+#    #+#             */
/*   Updated: 2026/03/27 16:20:08 by hwai-keo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "httpRequest.hpp"
#include <sstream>
#include <cstdlib>
#include <iostream>

HttpRequest::HttpRequest()
	: _method(""),
	  _path(""),
	  _version(""),
	  _body(""),
	  _contentLength(0),
	  _hasContentLength(false),
	  _headersParsed(false)
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

const std::string& HttpRequest::getVersion() const
{
	return _version;
}

const std::string* HttpRequest::getHeader(const std::string& key) const
{
	std::map<std::string, std::string>::const_iterator it = _headers.find(key);
	if (it == _headers.end())
		return NULL;
	return &it->second;
}

const std::string& HttpRequest::getBody() const
{
	return _body;
}

void HttpRequest::parse(const std::string& rawRequest)
{
	_method.clear();
	_path.clear();
	_version.clear();
	_body.clear();
	_headers.clear();
	_contentLength = 0;
	_hasContentLength = false;

	size_t headerEnd = rawRequest.find("\r\n\r\n");
	if (headerEnd == std::string::npos)
		throw std::runtime_error("Incomplete request");
	//header only parsed once
	size_t lineEnd = rawRequest.find("\r\n");
	if (lineEnd == std::string::npos)
		throw std::runtime_error("Invalid request line");

	std::string requestLine = rawRequest.substr(0, lineEnd);
	std::istringstream iss(requestLine);
		// split by empty line
	if (!(iss >> _method >> _path >> _version))
		throw std::runtime_error("Malformed request line");
	
	// validation
	if (_path.empty() || _path[0] != '/')
		throw std::runtime_error("invalid path");
	
	if (_version.find("HTTP/") != 0)
		throw std::runtime_error("invalid http version");

	std::string extra;
	if (iss >> extra)
		throw std::runtime_error("redundant token in request line");

	std::string headerSection = rawRequest.substr(0, headerEnd);
	std::istringstream stream(headerSection);
	std::string line;
	bool firstLine = true;

	// GET / HTTP/1.1\r\n
	// Host: localhost\r\n <- header start here
	// Content-Length: 5\r\n
	while (std::getline(stream, line))
	{
		// the getline will remove \n but not \r
		if (!line.empty() && line[line.length() - 1] == '\r')
			line.erase(line.length() - 1);
		// skip first line, and already parsed
		if (firstLine)
		{
			firstLine = false;
			continue;
		}
		if (line.empty())
			continue;

		// parse the host and content length
		size_t colonPos = line.find(':');
		if (colonPos == std::string::npos)
			throw std::runtime_error("malformed header");
		std::string key = line.substr(0, colonPos);
		std::string value = line.substr(colonPos + 1);

		// remove leading space
		if (key.empty())
			throw std::runtime_error("Malformed header: empty key");
		while (!value.empty() && (value[0] == ' ' || value[0] == '\t'))
			value.erase(0, 1);
		if (value.empty() && key == "Content-Length")
			throw std::runtime_error("Empty Content-Length");
		if (_headers.find(key) != _headers.end())
			throw std::runtime_error("Duplicate header");
		_headers[key] = value;
	}
	
	std::map<std::string, std::string>::const_iterator it = _headers.find("Content-Length");
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
		// convert str to number
		iss >> _contentLength;

		if (iss.fail())
			throw std::runtime_error("Invalid Content-Length");
		if (_contentLength > 1000000)
			throw std::runtime_error("Body too large");
		// if (_contentLength < 0)
		// 	throw std::runtime_error("Invalid Content-Length");
		_hasContentLength = true;
	}

	//body is after the headerEnd
	size_t bodyStart = headerEnd + 4;
	// anything exist after headers, treat it as body, if no content length
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

// bool	HttpRequest::isComplete() const
// {
// 	size_t headerEnd = _raw.find("\r\n\r\n");
// 	if (headerEnd == std::string::npos)
// 		return false;

// 	if (!_hasContentLength)
// 		return true;

// 	return _body.size() == _contentLength;
// }

bool HttpRequest::shouldCloseConnection() const
{
	std::map<std::string, std::string>::const_iterator it = _headers.find("Connection");

	if (it != _headers.end())
	{
		const std::string& value = it->second;

		if (value == "close")
			return true;
		if (value == "keep-alive")
			return false;
	}
	if (_version == "HTTP/1.1" || _version == "HTTP/1.0")
		return false;

	return true;
}