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

/*
ServerConfig (Beginner friendly):
One ServerConfig is one "server { ... }" section in the config file.

In real life, it answers questions like:
- Where do we listen? (host + port)
- Which folder do we serve files from? (root)
- What file is the default homepage? (index)
- How large can uploads/request bodies be? (max body size)
- Which custom HTML page do we show for errors? (error_page)
- What special rules exist for certain URL paths? (locations)

This class stores values only. The actual server code reads these values and
uses them to run sockets and handle HTTP requests.
*/

/*
Default Constructor:
Starts with empty/zero values.
After parsing, ConfigParser may fill missing values with defaults.
*/
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

/*
Destructor:
No dynamic ownership; containers clean themselves up.
*/
ServerConfig::~ServerConfig(void)
{
}

/*
setHost:
Sets the listening host (e.g. "127.0.0.1" or "0.0.0.0").
Validation is done by ConfigParser.
*/
void	ServerConfig::setHost(const std::string& host)
{
	_host = host;
}

/*
setPort:
Sets the listening TCP port.
Validation is done by ConfigParser.
*/
void	ServerConfig::setPort(int port)
{
	_port = port;
}

/*
setRoot:
Sets the server root directory.
Locations may override this with their own root.
*/
void	ServerConfig::setRoot(const std::string& root)
{
	_root = root;
}

/*
setIndex:
Sets the default index file name for the server.
Locations may override this with their own index.
*/
void	ServerConfig::setIndex(const std::string& index)
{
	_index = index;
}

/*
setMaxBodySize:
Sets the maximum request body size accepted by this server.
*/
void	ServerConfig::setMaxBodySize(size_t size)
{
	_maxBodySize = size;
}

/*
addErrorPage:
Sets a custom page for an HTTP error code.
Example: 404 -> /404.html
If set twice, the latest value replaces the old one.
*/
void	ServerConfig::addErrorPage(int code, const std::string& path)
{
	_errorPages[code] = path;
}

/*
addLocation:
Appends a fully parsed LocationConfig into the server.
*/
void	ServerConfig::addLocation(const LocationConfig& location)
{
	_locations.push_back(location);
}

/*
getHost:
Returns the server host.
*/
const std::string&	ServerConfig::getHost(void) const
{
	return _host;
}

/*
getPort:
Returns the server port.
*/
int	ServerConfig::getPort(void) const
{
	return _port;
}

/*
getRoot:
Returns the configured server root directory.
*/
const std::string&	ServerConfig::getRoot(void) const
{
	return _root;
}

/*
getIndex:
Returns the configured server index file name.
*/
const std::string&	ServerConfig::getIndex(void) const
{
	return _index;
}

/*
getMaxBodySize:
Returns the maximum allowed request body size.
*/
size_t	ServerConfig::getMaxBodySize(void) const
{
	return _maxBodySize;
}

/*
getErrorPage:
Returns the custom error page path for a code (example 404).
If there is no mapping, it returns NULL.
*/
const std::string*	ServerConfig::getErrorPage(int code) const
{
	std::map<int, std::string>::const_iterator it = _errorPages.find(code);
	if (it == _errorPages.end())
		return NULL;
	return &it->second;
}

/*
getLocations:
Returns the list of location blocks configured under this server.
*/
const std::vector<LocationConfig>&	ServerConfig::getLocations(void) const
{
	return _locations;
}
