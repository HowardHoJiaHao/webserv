/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   httpRequest.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: Ho Wai Keong <hwai_keo@student.42kl.edu    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/21 16:37:17 by hwai-keo          #+#    #+#             */
/*   Updated: 2026/04/05 11:35:39 by Ho Wai Keon      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <string>
#include <map>
#include <stdexcept>

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
		std::string trim(const std::string& str);
		void reset();
		void parseRequestLine(const std::string& requestLine);
		void parseHeaders(const std::string& headerSection);
		void validateHeaders(size_t maxBodySize);
		void extractBody(const std::string& rawRequest, size_t headerEnd);

	public:
		HttpRequest();

		//getter only(i only do read only acccess)
		const std::string& getMethod() const;
		const std::string& getPath() const;
		const std::string& getQuery() const;
		const std::string& getVersion() const;
		const std::string* getHeader(const std::string& key) const;
		const std::string& getBody() const;

		bool hasContentLength() const;
		size_t getContentLength() const;

		bool isComplete() const;

		void parse(const std::string& rawRequest, size_t maxBodySize);

		bool shouldCloseConnection() const;
		void parseCookies(const std::string& cookieHeader);
		std::string getCookie(const std::string& key) const;




};

#endif