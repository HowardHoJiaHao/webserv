/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParser.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ktiew <ktiew@student.42kl.edu.my>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/20 12:29:07 by ktiew             #+#    #+#             */
/*   Updated: 2026/02/21 23:54:27 by ktiew            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ConfigFiles.hpp"
#include "ConfigParser.hpp"
#include <algorithm>
#include <sstream>
#include <cstdlib>
#include <sys/stat.h>
#include <stdexcept>

bool	ConfigParser::isSupportedReturnStatus(int code)
{
	return (code >= 300 && code <= 399);
}

std::string	ConfigParser::applyPrefixPath(const std::string& prefix, const std::string& path)
{
	if (trim(prefix).empty() || trim(path).empty())
		return path;
	const bool	prefixEndsWithSlash = (!prefix.empty() && prefix[prefix.size() - 1] == '/');
	const bool	pathStartsWithSlash = (!path.empty() && path[0] == '/');
	if (prefixEndsWithSlash && pathStartsWithSlash)
		return prefix + path.substr(1);
	if (!prefixEndsWithSlash && !pathStartsWithSlash)
		return prefix + "/" + path;
	return prefix + path;
}

std::runtime_error	ConfigParser::parseError(size_t line, const std::string& message, int code)
{
	std::ostringstream	oss;
	oss << message << code;
	throw parseError(line, oss.str());
}

void	ConfigParser::parseListenValue(const std::string& listenValue, ServerConfig& server, size_t line)
{
	size_t	colon = listenValue.find(':');
	if (colon == std::string::npos)
	{
		unsigned long	port = parseUnsigned(listenValue, "listen", line);
		if (port > 65535)
			throw parseError(line, "invalid listen port: " + listenValue);
		server.setPort(static_cast<int>(port));
		if (server.getHost().empty())
			server.setHost("0.0.0.0");
		return;
	}
	std::string	host = listenValue.substr(0, colon);
	std::string	portStr = listenValue.substr(colon + 1);
	unsigned long port = parseUnsigned(portStr, "listen", line);
	if (port > 65535)
		throw parseError(line, "invalid listen port: " + portStr);
	server.setHost(host);
	server.setPort(static_cast<int>(port));
}

unsigned long	ConfigParser::parseUnsigned(const std::string& value, const std::string& fieldName, size_t line)
{
	char*	endptr = NULL;
	unsigned long	num = std::strtoul(value.c_str(), &endptr, 10);
	if (endptr == value.c_str() || *endptr != '\0')
		throw parseError(line, "invalid numeric value for " + fieldName + ": " + value);
	return num;
}

bool	ConfigParser::parseValidOnOff(const std::string& value, size_t line, const std::string& directive)
{
	const std::map<std::string, bool>&	map = getValidOnOffMap();
	std::map<std::string, bool>::const_iterator	it = map.find(value);
	if (it != map.end())
		return it->second;
	throw parseError(line, directive + " must be 'on/true/1' or 'off/false/0'");
}

void	ConfigParser::parseMethods(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix)
{
	(void) prefix;
	std::vector<std::string>	methods;
	while (i < tokens.size() && tokens[i].value != ";")
	{
		const std::string&	token = tokens[i].value;
		if (token != "GET" && token != "POST" && token != "DELETE")
			throw parseError(tokens[i].line, "invalid method '" + token + "' (expected GET/POST/DELETE)");
		if (std::find(methods.begin(), methods.end(), token) != methods.end())
			throw parseError(tokens[i].line, "duplicate method: " + token);
		methods.push_back(token);
		i++;
	}
	expectToken(tokens, i, ";");
	if (methods.empty())
		throw parseError(tokens[i - 1].line, "methods requires at least one method");
	loc.setAllowedMethods(methods);
}

void	ConfigParser::parseRoot(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix)
{
	if (i >= tokens.size())
		throw parseError(tokens[i - 1].line, "missing location root");
	loc.setRoot(applyPrefixPath(prefix, tokens[i++].value));
	expectToken(tokens, i, ";");
}

void	ConfigParser::parseUploadEnable(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix)
{
	(void) prefix;
	if (i >= tokens.size())
		throw parseError(tokens[i - 1].line, "missing upload_enable value");
	loc.setUploadEnabled(parseValidOnOff(tokens[i].value, tokens[i - 1].line, "upload_enable"));
	i++;
	expectToken(tokens, i, ";");
}

void	ConfigParser::parseUploadPath(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix)
{
	if (i >= tokens.size())
		throw parseError(tokens[i - 1].line, "missing upload_path value");
	loc.setUploadPath(applyPrefixPath(prefix, tokens[i++].value));
	expectToken(tokens, i, ";");
}

void	ConfigParser::parseAutoindex(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix)
{
	(void) prefix;
	if (i >= tokens.size())
		throw parseError(tokens[i - 1].line, "missing autoindex value");
	loc.setAutoindex(parseValidOnOff(tokens[i].value, tokens[i - 1].line, "autoindex"));
	i++;
	expectToken(tokens, i, ";");
}

void	ConfigParser::parseCgiEnabled(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix)
{
	(void) prefix;
	if (i >= tokens.size())
		throw parseError(tokens[i - 1].line, "missing cgi_enabled value");
	loc.setCgiEnabled(parseValidOnOff(tokens[i].value, tokens[i - 1].line, "cgi_enabled"));
	i++;
	expectToken(tokens, i, ";");
}

void	ConfigParser::parseCgiExtensions(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix)
{
	(void) prefix;
	if (i >= tokens.size())
		throw parseError(tokens[i - 1].line, "missing cgi_ext values");
	std::vector<std::string> extensions;
	while (i < tokens.size() && tokens[i].value != ";")
	{
		const std::string&	ext = tokens[i++].value;
		if (ext.size() < 2 || ext[0] != '.' || ext.find('/') != std::string::npos)
			throw parseError(tokens[i - 1].line, "invalid cgi_ext extension: " + ext);
		if (std::find(extensions.begin(), extensions.end(), ext) != extensions.end())
			throw parseError(tokens[i - 1].line, "duplicate cgi_ext extension: " + ext);
		extensions.push_back(ext);
	}
	expectToken(tokens, i, ";");
	if (extensions.empty())
		throw parseError(tokens[i - 1].line, "cgi_ext requires at least one extension");
	loc.setCgiExtensions(extensions);
}

void	ConfigParser::parseReturn(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix)
{
	(void) prefix;
	if (i >= tokens.size())
		throw parseError(tokens[i - 1].line, "missing return status code");
	unsigned long	codeUl = parseUnsigned(tokens[i].value, "return", tokens[i].line);
	i++;
	if (i >= tokens.size() || tokens[i].value == ";")
		throw parseError(tokens[i - 1].line, "missing return target");
	std::string	target = tokens[i++].value;
	expectToken(tokens, i, ";");
	if (codeUl > 999)
		throw parseError(tokens[i - 1].line, "invalid return status code");
	int	code = static_cast<int>(codeUl);
	if (!isSupportedReturnStatus(code))
		throw parseError(tokens[i - 1].line, "unsupported return status code", code);
	loc.setReturnDirective(true, code, target);
}

void	ConfigParser::parseListen(const std::vector<ConfigToken>& tokens, size_t& i, ServerConfig& server, const std::string& prefix)
{
	(void) prefix;
	if (i >= tokens.size())
		throw parseError(tokens[i - 1].line, "missing listen value");
	parseListenValue(tokens[i].value, server, tokens[i].line);
	i++;
	expectToken(tokens, i, ";");
}

void	ConfigParser::parseHost(const std::vector<ConfigToken>& tokens, size_t& i, ServerConfig& server, const std::string& prefix)
{
	(void) prefix;
	if (i >= tokens.size())
		throw parseError(tokens[i - 1].line, "missing host value");
	server.setHost(tokens[i++].value);
	expectToken(tokens, i, ";");
}

void	ConfigParser::parseRoot(const std::vector<ConfigToken>& tokens, size_t& i, ServerConfig& server, const std::string& prefix)
{
	if (i >= tokens.size())
		throw parseError(tokens[i - 1].line, "missing root value");
	server.setRoot(applyPrefixPath(prefix, tokens[i++].value));
	expectToken(tokens, i, ";");
}

void	ConfigParser::parseIndex(const std::vector<ConfigToken>& tokens, size_t& i, ServerConfig& server, const std::string& prefix)
{
	(void) prefix;
	if (i >= tokens.size())
		throw parseError(tokens[i - 1].line, "missing index value");
	server.setIndex(tokens[i++].value);
	expectToken(tokens, i, ";");
}

void	ConfigParser::parseClientMaxBodySize(const std::vector<ConfigToken>& tokens, size_t& i, ServerConfig& server, const std::string& prefix)
{
	(void) prefix;
	if (i >= tokens.size())
		throw parseError(tokens[i - 1].line, "missing client_max_body_size value");
	unsigned long	size = parseUnsigned(tokens[i].value, "client_max_body_size", tokens[i].line);
	i++;
	server.setMaxBodySize(static_cast<size_t>(size));
	expectToken(tokens, i, ";");
}

void	ConfigParser::parseErrorPage(const std::vector<ConfigToken>& tokens, size_t& i, ServerConfig& server, const std::string& prefix)
{
	if (i + 1 >= tokens.size())
		throw parseError(tokens[i - 1].line, "malformed error_page directive");
	unsigned long code = parseUnsigned(tokens[i].value, "error_page", tokens[i].line);
	i++;
	std::string	path = tokens[i++].value;
	server.addErrorPage(static_cast<int>(code), applyPrefixPath(prefix, path));
	expectToken(tokens, i, ";");
}

void	ConfigParser::parseLocationBlock(const std::vector<ConfigToken>& tokens, size_t& i, ServerConfig& server, const std::string& prefix)
{
	if (i >= tokens.size())
		throw parseError(tokens[tokens.size() - 1].line, "missing location path");
	LocationConfig	loc;
	loc.setPath(tokens[i++].value);
	expectToken(tokens, i, "{");
	const std::map<std::string, LocationDirectiveHandler>&	handlers = getLocationHandlers();
	while (i < tokens.size() && tokens[i].value != "}")
	{
		std::string key = tokens[i++].value;
		std::map<std::string, LocationDirectiveHandler>::const_iterator	it = handlers.find(key);
		if (it != handlers.end())
			it->second(tokens, i, loc, prefix);
		else
			throw parseError(tokens[i - 1].line, "unknown location directive '" + key + "'");
	}
	expectToken(tokens, i, "}");
	server.addLocation(loc);
}

const	std::map<std::string, bool>&	ConfigParser::getValidOnOffMap(void)
{
	static std::map<std::string, bool>	map;
	if (map.empty())
	{
		map["on"]		= true;
		map["true"]		= true;
		map["1"]		= true;
		map["off"]		= false;
		map["false"]	= false;
		map["0"]		= false;
	}
	return map;
}

const std::map<std::string, LocationDirectiveHandler>&	ConfigParser::getLocationHandlers(void)
{
	static std::map<std::string, LocationDirectiveHandler>	map;
	if (map.empty())
	{
		map["methods"]			= &parseMethods;
		map["allow_methods"]	= &parseMethods;
		map["root"]				= &parseRoot;
		map["upload_enable"]	= &parseUploadEnable;
		map["upload_path"]		= &parseUploadPath;
		map["autoindex"]		= &parseAutoindex;
		map["cgi_enabled"]		= &parseCgiEnabled;
		map["cgi_ext"]			= &parseCgiExtensions;
		map["cgi_extensions"]	= &parseCgiExtensions;
		map["return"]			= &parseReturn;
	}
	return map;
}

const std::map<std::string, ServerDirectiveHandler>&	ConfigParser::getServerHandlers(void)
{
	static std::map<std::string, ServerDirectiveHandler>	map;
	if (map.empty())
	{
		map["listen"]				= &parseListen;
		map["host"]					= &parseHost;
		map["root"]					= &parseRoot;
		map["index"]				= &parseIndex;
		map["client_max_body_size"]	= &parseClientMaxBodySize;
		map["error_page"]			= &parseErrorPage;
		map["location"]				= &parseLocationBlock;
	}
	return map;
}

std::string	ConfigParser::trim(const std::string& s)
{
	size_t	start = 0;
	while (start < s.size() && (s[start] == ' ' || s[start] == '\t' || s[start] == '\r' || s[start] == '\n'))
		start++;
	size_t	end = s.size();
	while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r' || s[end - 1] == '\n'))
		end--;
	return s.substr(start, end - start);
}

void	ConfigParser::expectToken(const std::vector<ConfigToken>& tokens, size_t& i, const std::string& expected)
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

bool	ConfigParser::isDirectoryPath(const std::string& path)
{
	struct stat	st;
	if (stat(path.c_str(), &st) != 0)
		return false;
	return S_ISDIR(st.st_mode);
}

std::vector<ConfigToken>	ConfigParser::tokenize(const std::string& content)
{
	std::vector<ConfigToken>	tokens;
	std::string	current;
	size_t	currentLine = 1;
	size_t	line = 1;
	for (size_t i = 0; i < content.size(); i++)
	{
		char	c = content[i];
		if (c == '\n')
			line++;
		if (c == '#')
		{
			while (i < content.size() && content[i] != '\n')
				i++;
			if (i < content.size() && content[i] == '\n')
				line++;
			if (!current.empty())
			{
				ConfigToken	token;
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
				ConfigToken	token;
				token.value = current;
				token.line = currentLine;
				tokens.push_back(token);
				current.clear();
			}
			ConfigToken	token;
			token.value = std::string(1, c);
			token.line = line;
			tokens.push_back(token);
			continue;
		}
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
		{
			if (!current.empty())
			{
				ConfigToken	token;
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
		ConfigToken	token;
		token.value = current;
		token.line = currentLine;
		tokens.push_back(token);
	}
	return tokens;
}

std::runtime_error	ConfigParser::parseError(size_t line, const std::string& message)
{
	std::ostringstream	oss;
	oss << "Config parse error at line " << line << ": " << message;
	return std::runtime_error(oss.str());
}

ServerConfig	ConfigParser::parseServerBlock(const std::vector<ConfigToken>& tokens, size_t& i, const std::string& prefix)
{
	ServerConfig	server;
	expectToken(tokens, i, "server");
	expectToken(tokens, i, "{");
	const std::map<std::string, ServerDirectiveHandler>&	handlers = getServerHandlers();
	while (i < tokens.size() && tokens[i].value != "}")
	{
		std::string key = tokens[i++].value;
		std::map<std::string, ServerDirectiveHandler>::const_iterator	it = handlers.find(key);
		if (it != handlers.end())
			it->second(tokens, i, server, prefix);
		else
			throw parseError(tokens[i - 1].line, "unknown server directive '" + key + "'");
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
