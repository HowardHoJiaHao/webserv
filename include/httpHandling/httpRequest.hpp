/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   httpRequest.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hwai-keo <hwai-keo@student.42kl.edu.my>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/21 16:37:17 by hwai-keo          #+#    #+#             */
/*   Updated: 2026/04/10 13:14:27 by hwai-keo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <string>
#include <map>
#include <stdexcept>

// one httpRequest = one client request
// client sends raw bytes to server.
// server parse it, make it to a structure object
// later using it to build response and send back to client
class HttpRequest
{
	private:
		std::string _method;
		std::string _path;
		std::string _query;
		std::string _version;
		//std::map<std::string, std::string> _headers;
		std::string _body;
		size_t		_contentLength;
		bool		_hasContentLength;

		std::map<std::string, std::string> _headers;
		std::map<std::string, std::string> _cookies;
		void reset();
		void parseRequestLine(const std::string& requestLine);
		void parseHeaders(const std::string& headerSection);
		void parseCookies();
		void validateHeaders(size_t maxBodySize);
		void extractBody(const std::string& rawRequest, size_t headerEnd, size_t maxBodySize);

	public:
		HttpRequest();

		//getter only(i only do read only acccess)
		const std::string& getMethod() const;
		const std::string& getPath() const;
		const std::string& getQuery() const;
		const std::string& getVersion() const;
		const std::string* getHeader(const std::string& key) const;
		const std::map<std::string, std::string>& getHeaders() const;
		const std::string* getCookieValue(const std::string& key) const;
		const std::map<std::string, std::string>& getCookies() const;
		const std::string& getBody() const;

		void parse(const std::string& rawRequest, size_t maxBodySize);

		bool shouldCloseConnectionByHttpRules() const;
};


#endif