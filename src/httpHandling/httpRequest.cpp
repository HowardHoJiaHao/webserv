/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   httpRequest.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hwai-keo <hwai-keo@student.42kl.edu.my>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/21 16:47:13 by hwai-keo          #+#    #+#             */
/*   Updated: 2026/03/23 16:28:45 by hwai-keo         ###   ########.fr       */
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
	_raw = rawRequest;
	size_t headerEnd = rawRequest.find("\r\n\r\n");
	if (headerEnd == std::string::npos)
		return;
	if (!_headersParsed)
	{
	// 	return ;
		_headers.clear();
		_contentLength = 0;
		_hasContentLength = false;
		//_body.clear();
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
			iss >> _contentLength;
			if (iss.fail())
				throw std::runtime_error("Invalid Content-Length");
			if (_contentLength < 0)
				throw std::runtime_error("Invalid Content-Length");
			_hasContentLength = true;
		}
		_headersParsed = true;
	}
		size_t bodyStart = headerEnd + 4;

		if (!_hasContentLength)
		{
			if (bodyStart < rawRequest.size())
				_body = rawRequest.substr(bodyStart);
		}
		else
		{
			size_t available = 0;
			if (rawRequest.size() > bodyStart)
				available = rawRequest.size() - bodyStart;
			if (available >= _contentLength)
				_body = rawRequest.substr(bodyStart, _contentLength);
			else
				_body.clear();
		}
	
}

bool	HttpRequest::isComplete() const
{
	size_t headerEnd = _raw.find("\r\n\r\n");
	if (headerEnd == std::string::npos)
		return false;

	if (!_hasContentLength)
		return true;

	return _body.size() == _contentLength;
}