/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerConfig.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: Ho Wai Keong <hwai_keo@student.42kl.edu    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/19 09:55:37 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/02/19 10:05:15 by Ho Wai Keon      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ServerConfig.hpp"

ServerConfig::ServerConfig()
	: _host(""),
	_port(0),
	_serverName(""),
	_root(""),
	_index(""),
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

void ServerConfig::setServerName(const std::string& name)
{
	_serverName = name;
}

void ServerConfig::setRoot(const std::string& root)
{
	_root = root;
}

void ServerConfig::setIndex(const std::string& index)
{
	_index = index;
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

const std::string& ServerConfig::getServerName() const
{
	return _serverName;
}

const std::string& ServerConfig::getRoot() const
{
	return _root;
}

const std::string& ServerConfig::getIndex() const
{
	return _index;
}

const std::vector<LocationConfig>& ServerConfig::getLocations() const
{
	return _locations;
}