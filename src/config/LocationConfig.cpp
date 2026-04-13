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

LocationConfig::~LocationConfig(void)
{
}

void	LocationConfig::setPath(const std::string& path)
{
	_path = path;
}

void	LocationConfig::setAllowedMethods(const std::vector<std::string>& methods)
{
	_allowedMethods = methods;
}

void	LocationConfig::setRoot(const std::string& root)
{
	_root = root;
}

void	LocationConfig::setIndex(const std::string& index)
{
	_index = index;
}

void	LocationConfig::setUploadEnabled(bool enabled)
{
	_uploadEnabled = enabled;
}

void	LocationConfig::setUploadPath(const std::string& path)
{
	_uploadPath = path;
}

void	LocationConfig::setAutoindex(bool enabled)
{
	_autoindex = enabled;
}

void	LocationConfig::setCgiEnabled(bool enabled)
{
	_cgiEnabled = enabled;
}

void	LocationConfig::setCgiExtensions(const std::vector<std::string>& extensions)
{
	_cgiExtensions = extensions;
}

void	LocationConfig::setReturnDirective(bool enabled, int status, const std::string& target)
{
	_hasReturn = enabled;
	_returnStatus = status;
	_returnTarget = target;
}

const std::string&	LocationConfig::getPath(void) const
{
	return _path;
}

const std::vector<std::string>&	LocationConfig::getAllowedMethods(void) const
{
	return _allowedMethods;
}

const std::string&	LocationConfig::getRoot(void) const
{
	return _root;
}

const std::string&	LocationConfig::getIndex(void) const
{
	return _index;
}

bool	LocationConfig::isUploadEnabled(void) const
{
	return _uploadEnabled;
}

const std::string&	LocationConfig::getUploadPath(void) const
{
	return _uploadPath;
}

bool	LocationConfig::isAutoindex(void) const
{
	return _autoindex;
}

bool	LocationConfig::isCgiEnabled(void) const
{
	return _cgiEnabled;
}

const std::vector<std::string>&	LocationConfig::getCgiExtensions(void) const
{
	return _cgiExtensions;
}

bool	LocationConfig::hasReturnDirective(void) const
{
	return _hasReturn;
}

int	LocationConfig::getReturnStatus(void) const
{
	return _returnStatus;
}

const std::string&	LocationConfig::getReturnTarget(void) const
{
	return _returnTarget;
}
