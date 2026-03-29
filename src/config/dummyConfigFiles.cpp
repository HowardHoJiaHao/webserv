/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   dummyConfigFiles.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: Ho Wai Keong <hwai_keo@student.42kl.edu    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/18 09:35:36 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/02/19 10:23:31 by Ho Wai Keon      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Webserv.hpp"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <sys/stat.h>

struct ConfigToken
{
	std::string value;
	size_t line;
};

static std::runtime_error parseError(size_t line, const std::string& message)
{
	std::ostringstream oss;
	oss << "Config parse error at line " << line << ": " << message;
	return std::runtime_error(oss.str());
}

static std::string trim(const std::string& s)
{
	size_t start = 0;
	while (start < s.size() && (s[start] == ' ' || s[start] == '\t' || s[start] == '\r' || s[start] == '\n'))
		++start;
	size_t end = s.size();
	while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r' || s[end - 1] == '\n'))
		--end;
	return s.substr(start, end - start);
}

static std::vector<ConfigToken> tokenizeConfig(const std::string& content)
{
	std::vector<ConfigToken> tokens;
	std::string current;
	size_t currentLine = 1;
	size_t line = 1;

	for (size_t i = 0; i < content.size(); ++i)
	{
		char c = content[i];
		if (c == '\n')
			++line;
		if (c == '#')
		{
			while (i < content.size() && content[i] != '\n')
				++i;
			if (i < content.size() && content[i] == '\n')
				++line;
			if (!current.empty())
			{
				ConfigToken token;
				token.value = current;
				token.line = currentLine;
				tokens.push_back(token);
				current.clear();
			}
			continue;
		}
		if (c == '{' || c == '}' || c == ';')
		{
			if (!current.empty())
			{
				ConfigToken token;
				token.value = current;
				token.line = currentLine;
				tokens.push_back(token);
				current.clear();
			}
			ConfigToken token;
			token.value = std::string(1, c);
			token.line = line;
			tokens.push_back(token);
			continue;
		}
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
		{
			if (!current.empty())
			{
				ConfigToken token;
				token.value = current;
				token.line = currentLine;
				tokens.push_back(token);
				current.clear();
			}
			continue;
		}
		if (current.empty())
			currentLine = line;
		current += c;
	}
	if (!current.empty())
	{
		ConfigToken token;
		token.value = current;
		token.line = currentLine;
		tokens.push_back(token);
	}
	return tokens;
}

static void expectToken(const std::vector<ConfigToken>& tokens, size_t& i, const std::string& expected)
{
	if (i >= tokens.size() || tokens[i].value != expected)
	{
		size_t line = 1;
		if (i < tokens.size())
			line = tokens[i].line;
		else if (!tokens.empty())
			line = tokens[tokens.size() - 1].line;
		throw parseError(line, "expected '" + expected + "'");
	}
	++i;
}

static unsigned long parseUnsigned(const std::string& value, const std::string& fieldName, size_t line)
{
	char* endptr = NULL;
	unsigned long num = std::strtoul(value.c_str(), &endptr, 10);
	if (endptr == value.c_str() || *endptr != '\0')
		throw parseError(line, "invalid numeric value for " + fieldName + ": " + value);
	return num;
}

static bool isDirectoryPath(const std::string& path)
{
	struct stat st;
	if (stat(path.c_str(), &st) != 0)
		return false;
	return S_ISDIR(st.st_mode);
}

static std::string applyPrefixPath(const std::string& prefix, const std::string& path)
{
	if (trim(prefix).empty() || trim(path).empty())
		return path;

	const bool prefixEndsWithSlash = (!prefix.empty() && prefix[prefix.size() - 1] == '/');
	const bool pathStartsWithSlash = (!path.empty() && path[0] == '/');

	if (prefixEndsWithSlash && pathStartsWithSlash)
		return prefix + path.substr(1);
	if (!prefixEndsWithSlash && !pathStartsWithSlash)
		return prefix + "/" + path;
	return prefix + path;
}

static void parseListenValue(const std::string& listenValue, ServerConfig& server, size_t line)
{
	size_t colon = listenValue.find(':');
	if (colon == std::string::npos)
	{
		unsigned long port = parseUnsigned(listenValue, "listen", line);
		if (port > 65535)
			throw parseError(line, "invalid listen port: " + listenValue);
		server.setPort(static_cast<int>(port));
		if (server.getHost().empty())
			server.setHost("0.0.0.0");
		return;
	}

	std::string host = listenValue.substr(0, colon);
	std::string portStr = listenValue.substr(colon + 1);
	unsigned long port = parseUnsigned(portStr, "listen", line);
	if (port > 65535)
		throw parseError(line, "invalid listen port: " + portStr);
	server.setHost(host);
	server.setPort(static_cast<int>(port));
}

static void parseLocationBlock(const std::vector<ConfigToken>& tokens, size_t& i, ServerConfig& server, const std::string& prefix)
{
	if (i >= tokens.size())
		throw parseError(tokens[tokens.size() - 1].line, "missing location path");

	LocationConfig loc;
	loc.setPath(tokens[i++].value);
	expectToken(tokens, i, "{");

	while (i < tokens.size() && tokens[i].value != "}")
	{
		std::string key = tokens[i++].value;
		if (key == "methods" || key == "allow_methods")
		{
			std::vector<std::string> methods;
			while (i < tokens.size() && tokens[i].value != ";")
				methods.push_back(tokens[i++].value);
			expectToken(tokens, i, ";");
			loc.setAllowedMethods(methods);
		}
		else if (key == "root")
		{
			if (i >= tokens.size())
				throw parseError(tokens[i - 1].line, "missing location root");
			loc.setRoot(applyPrefixPath(prefix, tokens[i++].value));
			expectToken(tokens, i, ";");
		}
		else if (key == "upload_enable")
		{
			if (i >= tokens.size())
				throw parseError(tokens[i - 1].line, "missing upload_enable value");
			std::string value = tokens[i++].value;
			loc.setUploadEnabled(value == "on" || value == "true" || value == "1");
			expectToken(tokens, i, ";");
		}
		else if (key == "upload_path")
		{
			if (i >= tokens.size())
				throw parseError(tokens[i - 1].line, "missing upload_path value");
			loc.setUploadPath(applyPrefixPath(prefix, tokens[i++].value));
			expectToken(tokens, i, ";");
		}
		else if (key == "autoindex")
		{
			if (i >= tokens.size())
				throw parseError(tokens[i - 1].line, "missing autoindex value");
			std::string value = tokens[i++].value;
			loc.setAutoindex(value == "on" || value == "true" || value == "1");
			expectToken(tokens, i, ";");
		}
		else
		{
			throw parseError(tokens[i - 1].line, "unknown location directive '" + key + "'");
		}
	}

	expectToken(tokens, i, "}");
	server.addLocation(loc);
}

static ServerConfig parseServerBlock(const std::vector<ConfigToken>& tokens, size_t& i, const std::string& prefix)
{
	ServerConfig server;
	expectToken(tokens, i, "server");
	expectToken(tokens, i, "{");

	while (i < tokens.size() && tokens[i].value != "}")
	{
		std::string key = tokens[i++].value;
		if (key == "listen")
		{
			if (i >= tokens.size())
				throw parseError(tokens[i - 1].line, "missing listen value");
			parseListenValue(tokens[i].value, server, tokens[i].line);
			++i;
			expectToken(tokens, i, ";");
		}
		else if (key == "host")
		{
			if (i >= tokens.size())
				throw parseError(tokens[i - 1].line, "missing host value");
			server.setHost(tokens[i++].value);
			expectToken(tokens, i, ";");
		}
		else if (key == "server_name")
		{
			if (i >= tokens.size())
				throw parseError(tokens[i - 1].line, "missing server_name value");
			server.setServerName(tokens[i++].value);
			expectToken(tokens, i, ";");
		}
		else if (key == "root")
		{
			if (i >= tokens.size())
				throw parseError(tokens[i - 1].line, "missing root value");
			server.setRoot(applyPrefixPath(prefix, tokens[i++].value));
			expectToken(tokens, i, ";");
		}
		else if (key == "index")
		{
			if (i >= tokens.size())
				throw parseError(tokens[i - 1].line, "missing index value");
			server.setIndex(tokens[i++].value);
			expectToken(tokens, i, ";");
		}
		else if (key == "client_max_body_size")
		{
			if (i >= tokens.size())
				throw parseError(tokens[i - 1].line, "missing client_max_body_size value");
			unsigned long size = parseUnsigned(tokens[i].value, "client_max_body_size", tokens[i].line);
			++i;
			server.setMaxBodySize(static_cast<size_t>(size));
			expectToken(tokens, i, ";");
		}
		else if (key == "error_page")
		{
			if (i + 1 >= tokens.size())
				throw parseError(tokens[i - 1].line, "malformed error_page directive");
			unsigned long code = parseUnsigned(tokens[i].value, "error_page", tokens[i].line);
			++i;
			std::string path = tokens[i++].value;
			server.addErrorPage(static_cast<int>(code), path);
			expectToken(tokens, i, ";");
		}
		else if (key == "location")
		{
			parseLocationBlock(tokens, i, server, prefix);
		}
		else
		{
			throw parseError(tokens[i - 1].line, "unknown server directive '" + key + "'");
		}
	}

	expectToken(tokens, i, "}");

	if (server.getPort() == 0)
		throw parseError(tokens[i - 1].line, "server missing listen/port");
	if (server.getHost().empty())
		server.setHost("0.0.0.0");
	if (trim(server.getRoot()).empty())
		server.setRoot(applyPrefixPath(prefix, "./www"));
	if (trim(server.getIndex()).empty())
		server.setIndex("index.html");

	return server;
}

ConfigFiles::ConfigFiles()
{
	_prefix.clear();
	initDummy();
}

ConfigFiles::ConfigFiles(const std::string& path)
{
	_prefix.clear();
	if (trim(path).empty())
		initDummy();
	else
		loadFromFile(path);
}

const std::vector<ServerConfig>& ConfigFiles::getServers() const
{
	return _serverConfigs;
}

const std::string& ConfigFiles::getPrefix() const
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
	std::vector<ConfigToken> tokens = tokenizeConfig(buffer.str());

	_serverConfigs.clear();
	size_t i = 0;
	_prefix.clear();
	if (i < tokens.size() && tokens[i].value == "prefix")
	{
		size_t line = tokens[i++].line;
		if (i >= tokens.size() || tokens[i].value == ";" || tokens[i].value == "{" || tokens[i].value == "}")
			throw parseError(line, "missing prefix path");
		_prefix = tokens[i++].value;
		expectToken(tokens, i, ";");
		if (!isDirectoryPath(_prefix))
			throw parseError(line, "prefix must be an existing directory: " + _prefix);
	}
	else if (i < tokens.size() && tokens[i].value != "server")
	{
		throw parseError(tokens[i].line, "expected 'prefix' or 'server' block");
	}
	while (i < tokens.size())
	{
		if (tokens[i].value != "server")
			throw parseError(tokens[i].line, "expected 'server' block");
		ServerConfig server = parseServerBlock(tokens, i, _prefix);
		_serverConfigs.push_back(server);
	}

	if (_serverConfigs.empty())
		throw std::runtime_error("Config parse error: no server block found");
}

void ConfigFiles::initDummy()
{
	// first server
	ServerConfig server1;
	server1.setHost("127.0.0.1");
	server1.setPort(8080);
	server1.setServerName("test1");
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
	server2.setServerName("test2");
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