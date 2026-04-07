/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   httpRequest.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hwai-keo <hwai-keo@student.42kl.edu.my>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/21 16:47:13 by hwai-keo          #+#    #+#             */
/*   Updated: 2026/04/07 15:43:43 by hwai-keo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "httpRequest.hpp"
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

std::string HttpRequest::getCookie(const std::string& key) const
{
	std::map<std::string, std::string>::const_iterator it = _cookies.find(key);
	if (it != _cookies.end())
		return it->second;
	return "";
}