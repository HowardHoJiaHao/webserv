/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   httpRequest.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hwai-keo <hwai-keo@student.42kl.edu.my>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/21 16:47:13 by hwai-keo          #+#    #+#             */
/*   Updated: 2026/04/08 17:29:26 by hwai-keo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "httpRequest.hpp"
#include "engine_string_utils.hpp"

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
	std::map<std::string, std::string>::const_iterator it = _headers.find(toLowerAsciiEngine(key));
	if (it == _headers.end())
		return NULL;
	return &it->second;
}

const std::map<std::string, std::string>& HttpRequest::getHeaders() const
{
	return _headers;
}

const std::string* HttpRequest::getCookie(const std::string& key) const
{
	std::map<std::string, std::string>::const_iterator it = _cookies.find(toLowerAsciiEngine(key));
	if (it == _cookies.end())
		return NULL;
	return &it->second;
}

const std::map<std::string, std::string>& HttpRequest::getCookies() const
{
	return _cookies;
}

const std::string& HttpRequest::getBody() const
{
	return _body;
}



// example of header map
// _headers =
// {
//		"host"				→ "example.com",
//		"user-agent"		→ "Mozilla/5.0",
//		"content-length"	→ "11",
//		"connection"		→ "keep-alive",
//		"cookie"			→ "session=abc123; theme=dark"
// }

bool HttpRequest::shouldCloseConnectionByHttpRules() const
{
	std::map<std::string, std::string>::const_iterator it = _headers.find("connection");

	if (it != _headers.end())
	{
		const std::string value = toLowerAsciiEngine(it->second);

		if (value == "close")
			return true;
		if (value == "keep-alive")
			return false;
	}
	if (_version == "HTTP/1.1")
		return false;
	return true;
}

