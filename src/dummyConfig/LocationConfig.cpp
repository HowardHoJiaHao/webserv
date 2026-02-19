/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   LocationConfig.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: Ho Wai Keong <hwai_keo@student.42kl.edu    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/19 09:55:21 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/02/19 10:14:16 by Ho Wai Keon      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "LocationConfig.hpp"

LocationConfig::LocationConfig()
	: _path(""),
	_allowedMethods(),
	_root(""),
	_uploadEnabled(false),
	_uploadPath("")
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
