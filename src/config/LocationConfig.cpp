/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   LocationConfig.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ktiew <ktiew@student.42kl.edu.my>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/20 12:29:07 by ktiew             #+#    #+#             */
/*   Updated: 2026/02/21 23:54:27 by ktiew            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "LocationConfig.hpp"

/*
LocationConfig (Beginner friendly):
A "location" is a rule for a URL path.

Example:
- location /Upload { ... }
	applies when the client requests a URL that starts with /Upload.

This class is just a container of settings for that location.
It does not do networking by itself.

The request-handling code later reads these settings to decide things like:
- which HTTP methods are allowed (GET/POST/DELETE)
- whether uploads are allowed and where files should be stored
- whether directory listing (autoindex) should be shown
- whether CGI scripts can run
- whether to immediately redirect (return directive)
*/

/*
Default Constructor:
Starts with everything "off" or empty.
Then ConfigParser sets only the fields that appear in the config file.
*/
LocationConfig::LocationConfig(void)
	: _path(""),
	_allowedMethods(),
	_root(""),
	_index(""),
	_uploadEnabled(false),
	_uploadPath(""),
	_autoindex(false),
	_cgiEnabled(false),
	_cgiExtensions(),
	_hasReturn(false),
	_returnStatus(0),
	_returnTarget("")
{
}

/*
Destructor:
No dynamic ownership; vectors/strings clean themselves up.
*/
LocationConfig::~LocationConfig(void)
{
}

/*
setPath:
Stores the location matching path as written in the config.
*/
void	LocationConfig::setPath(const std::string& path)
{
	_path = path;
}

/*
setAllowedMethods:
Stores which HTTP methods are allowed for this path.
If a method is not listed, the server should reject it for this location.
*/
void	LocationConfig::setAllowedMethods(const std::vector<std::string>& methods)
{
	_allowedMethods = methods;
}

/*
setRoot:
Sets the folder on disk that this location serves files from.
If empty, the server can fall back to the server-wide root.
*/
void	LocationConfig::setRoot(const std::string& root)
{
	_root = root;
}

/*
setIndex:
Sets the default file name when a directory is requested (example: index.html).
*/
void	LocationConfig::setIndex(const std::string& index)
{
	_index = index;
}

/*
setUploadEnabled:
Turns uploads on/off for this location.
*/
void	LocationConfig::setUploadEnabled(bool enabled)
{
	_uploadEnabled = enabled;
}

/*
setUploadPath:
Where uploaded files should be saved on disk.
*/
void	LocationConfig::setUploadPath(const std::string& path)
{
	_uploadPath = path;
}

/*
setAutoindex:
If true, the server may show a directory listing when no index file exists.
*/
void	LocationConfig::setAutoindex(bool enabled)
{
	_autoindex = enabled;
}

/*
setCgiEnabled:
If true, the server may execute CGI scripts for matching requests.
*/
void	LocationConfig::setCgiEnabled(bool enabled)
{
	_cgiEnabled = enabled;
}

/*
setCgiExtensions:
Which file extensions are treated as CGI scripts (example: .py, .pl).
*/
void	LocationConfig::setCgiExtensions(const std::vector<std::string>& extensions)
{
	_cgiExtensions = extensions;
}

/*
setReturnDirective:
Stores a redirect rule like:
	return 301 /new-page;

When enabled, the server can respond immediately with that redirect.
*/
void	LocationConfig::setReturnDirective(bool enabled, int status, const std::string& target)
{
	_hasReturn = enabled;
	_returnStatus = status;
	_returnTarget = target;
}

/*
getPath:
Returns the location match string.
*/
const std::string&	LocationConfig::getPath(void) const
{
	return _path;
}

/*
getAllowedMethods:
Returns the list of allowed methods for this location.
*/
const std::vector<std::string>&	LocationConfig::getAllowedMethods(void) const
{
	return _allowedMethods;
}

/*
getRoot:
Returns the location root (may be empty).
*/
const std::string&	LocationConfig::getRoot(void) const
{
	return _root;
}

/*
getIndex:
Returns the location index file name (may be empty).
*/
const std::string&	LocationConfig::getIndex(void) const
{
	return _index;
}

/*
isUploadEnabled:
True when uploads are enabled for this location.
*/
bool	LocationConfig::isUploadEnabled(void) const
{
	return _uploadEnabled;
}

/*
getUploadPath:
Returns the configured upload directory path.
*/
const std::string&	LocationConfig::getUploadPath(void) const
{
	return _uploadPath;
}

/*
isAutoindex:
True when directory listing is allowed.
*/
bool	LocationConfig::isAutoindex(void) const
{
	return _autoindex;
}

/*
isCgiEnabled:
True when CGI is enabled for this location.
*/
bool	LocationConfig::isCgiEnabled(void) const
{
	return _cgiEnabled;
}

/*
getCgiExtensions:
Returns allowed CGI extensions for this location.
*/
const std::vector<std::string>&	LocationConfig::getCgiExtensions(void) const
{
	return _cgiExtensions;
}

/*
hasReturnDirective:
True when a "return" directive was configured.
*/
bool	LocationConfig::hasReturnDirective(void) const
{
	return _hasReturn;
}

/*
getReturnStatus:
Returns the HTTP status code configured for the return directive.
*/
int	LocationConfig::getReturnStatus(void) const
{
	return _returnStatus;
}

/*
getReturnTarget:
Returns the redirect/target string configured for the return directive.
*/
const std::string&	LocationConfig::getReturnTarget(void) const
{
	return _returnTarget;
}
