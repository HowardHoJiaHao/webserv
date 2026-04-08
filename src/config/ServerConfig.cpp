/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerConfig.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ho <hwai-keo@student.42kl.edu.my>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/19 09:55:37 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/03/28 00:34:17 by ho               ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ServerConfig.hpp"

ServerConfig::ServerConfig()
	: _host(""),
	_port(0),
	_root(""),
	_index(""),
	_maxBodySize(1000000),
	_errorPages(),
	_locations()
	{}

void ServerConfig::setHost(const std::string& host)
{
	_host = host;
}

void ServerConfig::setPort(int port)
{
	_port = port;
}



void ServerConfig::setRoot(const std::string& root)
{
	_root = root;
}

void ServerConfig::setIndex(const std::string& index)
{
	_index = index;
}

void ServerConfig::setMaxBodySize(size_t size)
{
	_maxBodySize = size;
}

void ServerConfig::addErrorPage(int code, const std::string& path)
{
	_errorPages[code] = path;
}

void ServerConfig::addLocation(const LocationConfig& location)
{
	_locations.push_back(location);
}

const std::string& ServerConfig::getHost() const
{
	return _host;
}

int ServerConfig::getPort() const
{
	return _port;
}



const std::string& ServerConfig::getRoot() const
{
	return _root;
}

const std::string& ServerConfig::getIndex() const
{
	return _index;
}

size_t ServerConfig::getMaxBodySize() const
{
	return _maxBodySize;
}

const std::string* ServerConfig::getErrorPage(int code) const
{
	std::map<int, std::string>::const_iterator it = _errorPages.find(code);
	if (it == _errorPages.end())
		return NULL;
	return &it->second;
}

const std::vector<LocationConfig>& ServerConfig::getLocations() const
{
	return _locations;
}