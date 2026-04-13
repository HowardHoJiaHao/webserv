/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerConfig.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ktiew <ktiew@student.42kl.edu.my>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/20 12:29:07 by ktiew             #+#    #+#             */
/*   Updated: 2026/02/21 23:54:27 by ktiew            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ServerConfig.hpp"

ServerConfig::ServerConfig(void)
	: _host(""),
	_port(0),
	_root(""),
	_index(""),
	_maxBodySize(1000000),
	_errorPages(),
	_locations()
{
}

ServerConfig::~ServerConfig(void)
{
}

void	ServerConfig::setHost(const std::string& host)
{
	_host = host;
}

void	ServerConfig::setPort(int port)
{
	_port = port;
}

void	ServerConfig::setRoot(const std::string& root)
{
	_root = root;
}

void	ServerConfig::setIndex(const std::string& index)
{
	_index = index;
}

void	ServerConfig::setMaxBodySize(size_t size)
{
	_maxBodySize = size;
}

void	ServerConfig::addErrorPage(int code, const std::string& path)
{
	_errorPages[code] = path;
}

void	ServerConfig::addLocation(const LocationConfig& location)
{
	_locations.push_back(location);
}

const std::string&	ServerConfig::getHost(void) const
{
	return _host;
}

int	ServerConfig::getPort(void) const
{
	return _port;
}

const std::string&	ServerConfig::getRoot(void) const
{
	return _root;
}

const std::string&	ServerConfig::getIndex(void) const
{
	return _index;
}

size_t	ServerConfig::getMaxBodySize(void) const
{
	return _maxBodySize;
}

const std::string*	ServerConfig::getErrorPage(int code) const
{
	std::map<int, std::string>::const_iterator it = _errorPages.find(code);
	if (it == _errorPages.end())
		return NULL;
	return &it->second;
}

const std::vector<LocationConfig>&	ServerConfig::getLocations(void) const
{
	return _locations;
}
