/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   LocationConfig.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ho <hwai-keo@student.42kl.edu.my>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/19 09:55:21 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/03/28 00:34:17 by ho               ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "LocationConfig.hpp"

LocationConfig::LocationConfig()
	: _path(""),
	_allowedMethods(),
	_root(""),
	_uploadEnabled(false),
	_uploadPath(""),
	_autoindex(false)
	{}

void LocationConfig::setPath(const std::string& path)
{
	_path = path;
}

void LocationConfig::setAllowedMethods(const std::vector<std::string>& methods)
{
	_allowedMethods = methods;
}

void LocationConfig::setRoot(const std::string& root)
{
	_root = root;
}

void LocationConfig::setUploadEnabled(bool enabled)
{
	_uploadEnabled = enabled;
}

void LocationConfig::setUploadPath(const std::string& path)
{
	_uploadPath = path;
}

void LocationConfig::setAutoindex(bool enabled)
{
	_autoindex = enabled;
}

const std::string& LocationConfig::getPath() const
{
	return _path;
}

const std::vector<std::string>& LocationConfig::getAllowedMethods() const
{
	return _allowedMethods;
}

const std::string& LocationConfig::getRoot() const
{
	return _root;
}

bool LocationConfig::isUploadEnabled() const
{
	return _uploadEnabled;
}

const std::string& LocationConfig::getUploadPath() const
{
	return _uploadPath;
}

bool LocationConfig::isAutoindex() const
{
	return _autoindex;
}
