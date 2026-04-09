/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigFiles.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ktiew <ktiew@student.42kl.edu.my>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/20 12:29:07 by ktiew             #+#    #+#             */
/*   Updated: 2026/02/21 23:54:27 by ktiew            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ConfigFiles.hpp"
#include "ConfigParser.hpp"
#include <fstream>
#include <sstream>

ConfigFiles::ConfigFiles(void)
	: _prefix("")
{
	initDefault();
}

ConfigFiles::ConfigFiles(const std::string& path)
	: _prefix("")
{
	if (ConfigParser::trim(path).empty())
		initDefault();
	else
		loadFromFile(path);
}

ConfigFiles::~ConfigFiles(void)
{
}

const std::vector<ServerConfig>& ConfigFiles::getServers(void) const
{
	return _serverConfigs;
}

const std::string& ConfigFiles::getPrefix(void) const
{
	return _prefix;
}

void ConfigFiles::loadFromFile(const std::string& path)
{
	std::ifstream in(path.c_str());
	if (!in.is_open())
		throw std::runtime_error("Cannot open config file: " + path);

	std::stringstream buffer;
	buffer << in.rdbuf();
	std::vector<ConfigToken> tokens = ConfigParser::tokenize(buffer.str());

	_serverConfigs.clear();
	size_t i = 0;
	if (i < tokens.size() && tokens[i].value == "prefix")
	{
		size_t line = tokens[i++].line;
		if (i >= tokens.size() || tokens[i].value == ";" || tokens[i].value == "{" || tokens[i].value == "}")
		{
			std::ostringstream _oss;
			_oss << "Config parse error at line " << line << ": missing prefix path";
			throw std::runtime_error(_oss.str());
		}
		_prefix = tokens[i++].value;
		ConfigParser::expectToken(tokens, i, ";");
		if (!ConfigParser::isDirectoryPath(_prefix))
		{
			std::ostringstream _oss;
			_oss << "Config parse error at line " << line << ": prefix must be an existing directory: " << _prefix;
			throw std::runtime_error(_oss.str());
		}
	}
	else if (i < tokens.size() && tokens[i].value != "server")
	{
		{
			std::ostringstream _oss;
			_oss << "Config parse error at line " << tokens[i].line << ": expected 'prefix' or 'server' block";
			throw std::runtime_error(_oss.str());
		}
	}
	while (i < tokens.size())
	{
		if (tokens[i].value != "server")
			{
				std::ostringstream _oss;
				_oss << "Config parse error at line " << tokens[i].line << ": expected 'server' block";
				throw std::runtime_error(_oss.str());
			}
		ServerConfig server = ConfigParser::parseServerBlock(tokens, i, _prefix);
		_serverConfigs.push_back(server);
	}

	if (_serverConfigs.empty())
		throw std::runtime_error("Config parse error: no server block found");
}

void ConfigFiles::initDefault(void)
{
	// first server
	ServerConfig server1;
	server1.setHost("127.0.0.1");
	server1.setPort(8080);
	server1.setRoot("./www1");
	server1.setIndex("index.html");
	server1.setMaxBodySize(1000000);
	server1.addErrorPage(404, "/404.html");
	server1.addErrorPage(500, "/500.html");

	LocationConfig loc1;
	loc1.setPath("/");
	{
		std::vector<std::string> methods;
		methods.push_back("GET");
		methods.push_back("POST");
		loc1.setAllowedMethods(methods);
	}
	loc1.setUploadEnabled(false);
	loc1.setAutoindex(false);
	
	LocationConfig loc2;
	loc2.setPath("/Upload");
	{
		std::vector<std::string> methods;
		methods.push_back("POST");
		loc2.setAllowedMethods(methods);
	}
	loc2.setUploadEnabled(true);
	loc2.setUploadPath("./uploads");
	loc2.setAutoindex(false);

	server1.addLocation(loc1);
	server1.addLocation(loc2);
	_serverConfigs.push_back(server1);

	//second server
	ServerConfig server2;
	server2.setHost("127.0.0.1");
	server2.setPort(8081);
	server2.setRoot("./www2");
	server2.setIndex("home.html");
	server2.setMaxBodySize(1000000);
	server2.addErrorPage(404, "/404.html");

	LocationConfig loc3;
	loc3.setPath("/");
	{
		std::vector<std::string> methods;
		methods.push_back("GET");
		loc3.setAllowedMethods(methods);
	}
	loc3.setUploadEnabled(false);
	loc3.setAutoindex(true);

	server2.addLocation(loc3);
	_serverConfigs.push_back(server2);
}