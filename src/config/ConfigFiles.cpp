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

const std::vector<ServerConfig>&	ConfigFiles::getServers(void) const
{
	return _serverConfigs;
}

const std::string&	ConfigFiles::getPrefix(void) const
{
	return _prefix;
}

void	ConfigFiles::setPrefix(const std::string& prefix)
{
	_prefix = prefix;
}

void	ConfigFiles::addServer(const ServerConfig& server)
{
	_serverConfigs.push_back(server);
}

std::string	ConfigFiles::readFromFile(const std::string& path)
{
	std::ifstream	in(path.c_str());

	if (!in.is_open())
		throw std::runtime_error("Cannot open config file: " + path);

	std::ostringstream	buffer;

	buffer << in.rdbuf();
	return buffer.str();
}

void	ConfigFiles::initDefault(void)
{
	ServerConfig	server1;

	server1.setHost("127.0.0.1");
	server1.setPort(8080);
	server1.setRoot("./www1");
	server1.setIndex("index.html");
	server1.setMaxBodySize(1000000);
	server1.addErrorPage(404, "/404.html");
	server1.addErrorPage(500, "/500.html");

	LocationConfig	loc1;

	loc1.setPath("/");
	{
		std::vector<std::string>	methods;

		methods.push_back("GET");
		methods.push_back("POST");
		loc1.setAllowedMethods(methods);
	}
	loc1.setUploadEnabled(false);
	loc1.setAutoindex(false);
	
	LocationConfig	loc2;

	loc2.setPath("/Upload");
	{
		std::vector<std::string>	methods;

		methods.push_back("POST");
		loc2.setAllowedMethods(methods);
	}
	loc2.setUploadEnabled(true);
	loc2.setUploadPath("./uploads");
	loc2.setAutoindex(false);

	server1.addLocation(loc1);
	server1.addLocation(loc2);
	_serverConfigs.push_back(server1);

	ServerConfig	server2;

	server2.setHost("127.0.0.1");
	server2.setPort(8081);
	server2.setRoot("./www2");
	server2.setIndex("home.html");
	server2.setMaxBodySize(1000000);
	server2.addErrorPage(404, "/404.html");

	LocationConfig	loc3;

	loc3.setPath("/");
	{
		std::vector<std::string>	methods;

		methods.push_back("GET");
		loc3.setAllowedMethods(methods);
	}
	loc3.setUploadEnabled(false);
	loc3.setAutoindex(true);

	server2.addLocation(loc3);
	_serverConfigs.push_back(server2);
}

void	ConfigFiles::loadFromFile(const std::string& path)
{
	std::vector<ConfigParser::ConfigToken>	tokens = ConfigParser::tokenize(readFromFile(path));

	_serverConfigs.clear();
	size_t	i = 0;

	const std::map<std::string, ConfigParser::TopLevelDirectiveHandler>&	handlers = ConfigParser::getTopLevelHandlers();

	while (i < tokens.size())
	{
		std::string	key = tokens[i].value;
		std::map<std::string, ConfigParser::TopLevelDirectiveHandler>::const_iterator	it = handlers.find(key);

		if (it != handlers.end())
			it->second(tokens, i, *this);
		else
			throw ConfigParser::parseError(tokens[i].line, "unknown top-level directive '" + key + "'");
	}
	if (_serverConfigs.empty())
		throw std::runtime_error("Config parse error: no server block");
}
