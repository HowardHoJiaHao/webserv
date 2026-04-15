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

/*
ConfigFiles (Beginner friendly):
Think of this class as the "final result" of reading a webserv config file.

After the config is loaded, the rest of the program can ask:
- "What servers do I run?" (host/port/root/index/...)
- "What locations exist inside each server?" (methods, uploads, CGI, ...)

This class supports two ways to get a configuration:
1) Built-in default config (useful when you run the program without a file)
2) Load and parse a config file (using ConfigParser)
*/

/*
Default Constructor:
Builds a config using hard-coded defaults.
This helps beginners/testers run the server quickly without writing a config.
*/
ConfigFiles::ConfigFiles(void)
	: _prefix("")
{
	initDefault();
}

/*
Path Constructor:
- If the path is empty (or just spaces), use the built-in defaults.
- Otherwise, read that file and parse it into server/location settings.
*/
ConfigFiles::ConfigFiles(const std::string& path)
	: _prefix("")
{
	if (ConfigParser::trim(path).empty())
		initDefault();
	else
		loadFromFile(path);
}

/*
Destructor:
Nothing special to free (we only use std::string/std::vector).
*/
ConfigFiles::~ConfigFiles(void)
{
}

/*
getServers:
Gives you all configured servers.
Each ServerConfig is one "server { ... }" block from the config.
*/
const std::vector<ServerConfig>&	ConfigFiles::getServers(void) const
{
	return _serverConfigs;
}

/*
getPrefix:
Returns the optional prefix directory.
If it is not set, it will be an empty string.
*/
const std::string&	ConfigFiles::getPrefix(void) const
{
	return _prefix;
}

/*
setPrefix:
Stores the prefix directory.
Note: ConfigParser validates whether it exists.
*/
void	ConfigFiles::setPrefix(const std::string& prefix)
{
	_prefix = prefix;
}

/*
addServer:
Adds one parsed server block into the list.
ConfigParser calls this after it finishes reading a "server { ... }" section.
*/
void	ConfigFiles::addServer(const ServerConfig& server)
{
	_serverConfigs.push_back(server);
}

/*
readFromFile:
Reads the whole config file into one big string.
If the file cannot be opened, throws an exception (std::runtime_error).
*/
std::string	ConfigFiles::readFromFile(const std::string& path)
{
	std::ifstream	in(path.c_str());

	if (!in.is_open())
		throw std::runtime_error("Cannot open config file: " + path);

	std::ostringstream	buffer;

	buffer << in.rdbuf();
	return buffer.str();
}

/*
initDefault:
Creates a small default setup:
- server on 127.0.0.1:8080 serving ./www1
- server on 127.0.0.1:8081 serving ./www2

This is mainly for local testing and learning.
*/
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

/*
loadFromFile:
Loads and parses a config file.

Beginner view of what happens:
1) Read file text
2) Split it into "tokens" (words and symbols like '{' '}' ';')
3) Walk through tokens and handle top-level commands like "prefix" and "server"

It will:
- Clear any old servers before parsing
- Throw a clear error if it sees an unknown top-level command
- Throw an error if there are no server blocks at all
*/
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
