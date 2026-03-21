/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   httpRequest.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hwai-keo <hwai-keo@student.42kl.edu.my>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/21 16:47:13 by hwai-keo          #+#    #+#             */
/*   Updated: 2026/03/21 18:56:27 by hwai-keo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "httpRequest.hpp"
#include <sstream>
#include <iostream>

HttpRequest::HttpRequest()
	: _method(""),
	  _path(""),
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
	_headers.clear();
	size_t pos = rawRequest.find("\r\n");
	if (pos == std::string::npos)
		throw std::runtime_error("Invalid request line");
	std::string requestLine = rawRequest.substr(0, pos);

	std::istringstream iss(requestLine);
	if (!(iss >> _method >> _path >> _version))
		throw std::runtime_error("Malformed request line");
	
	if (_path.empty() || _path[0] != '/')
		throw std::runtime_error("invalid path");
	
	if (_version.find("HTTP/") != 0)
		throw std::runtime_error("invalid http version");

	std::string extra;
	if (iss >> extra)
		throw std::runtime_error("redundant token in request line");

	size_t headerEnd = rawRequest.find("\r\n\r\n");
	if (headerEnd == std::string::npos)
		throw std::runtime_error("Headers not complete");
	std::string headerSection = rawRequest.substr(0, headerEnd);
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
		size_t colonPos = line.find(':');
		if (colonPos == std::string::npos)
			throw std::runtime_error("malformed header");
		std::string key = line.substr(0, colonPos);
		std::string value = line.substr(colonPos + 1);

		if (!value.empty() && value[0] == ' ')
			value.erase(0, 1);
		_headers[key] = value; 
	}
}

// const std::map<std::string, std::string>& getHeaders() const
// {
	
// }