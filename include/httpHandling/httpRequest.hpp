/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   httpRequest.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hwai-keo <hwai-keo@student.42kl.edu.my>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/21 16:37:17 by hwai-keo          #+#    #+#             */
/*   Updated: 2026/03/27 14:35:15 by hwai-keo         ###   ########.fr       */
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
		std::string _version;
		std::map<std::string, std::string> _headers;
		std::string _raw;
		std::string _body;
		size_t		_contentLength;
		bool		_hasContentLength;
		bool		_headersParsed;

	public:
		HttpRequest();

		//getter only(i only do read only acccess)
		const std::string& getMethod() const;
		const std::string& getPath() const;
		const std::string& getVersion() const;
		const std::string* getHeader(const std::string& key) const;
		const std::string& getBody() const;

		bool hasContentLength() const;
		size_t getContentLength() const;

		bool isComplete() const;

		void parse(const std::string& rawRequest);

		bool shouldCloseConnection() const;



};

#endif