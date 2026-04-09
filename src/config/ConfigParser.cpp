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

static unsigned long parseUnsigned(const std::string& value, const std::string& fieldName, size_t line)
{
	char* endptr = NULL;
	unsigned long num = std::strtoul(value.c_str(), &endptr, 10);
	if (endptr == value.c_str() || *endptr != '\0')
		throw ConfigParser::parseError(line, "invalid numeric value for " + fieldName + ": " + value);
	return num;
}

static bool isSupportedReturnStatus(int code)
{
	return (code >= 300 && code <= 399);
}

static std::string applyPrefixPath(const std::string& prefix, const std::string& path)
{
	if (ConfigParser::trim(prefix).empty() || ConfigParser::trim(path).empty())
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
			throw ConfigParser::parseError(line, "invalid listen port: " + listenValue);
		server.setPort(static_cast<int>(port));
		if (server.getHost().empty())
			server.setHost("0.0.0.0");
		return;
	}
	std::string host = listenValue.substr(0, colon);
	std::string portStr = listenValue.substr(colon + 1);
	unsigned long port = parseUnsigned(portStr, "listen", line);
	if (port > 65535)
		throw ConfigParser::parseError(line, "invalid listen port: " + portStr);
	server.setHost(host);
	server.setPort(static_cast<int>(port));
}

bool	parseValidOnOff(const std::string& value, size_t line, const std::string& directive)
{
	static std::vector<std::string>	validOn;
	static std::vector<std::string>	validOff;
	validOn.push_back("on");
	validOn.push_back("true");
	validOn.push_back("1");
	validOff.push_back("off");
	validOff.push_back("false");
	validOff.push_back("0");
	if (std::find(validOn.begin(), validOn.end(), value) != validOn.end())
		return true;
	if (std::find(validOff.begin(), validOff.end(), value) != validOff.end())
		return false;
	throw ConfigParser::parseError(line, directive + " must be 'on/true/1' or 'off/false/0'");
}

void	parseMethods(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix)
{
	(void) prefix;
	std::vector<std::string>	methods;
	while (i < tokens.size() && tokens[i].value != ";")
	{
		const std::string&	token = tokens[i].value;
		if (token == "GET" || token == "POST" || token == "DELETE")
		{
			if (std::find(methods.begin(), methods.end(), token) != methods.end())
				throw ConfigParser::parseError(tokens[i].line, "duplicate method: " + token);
			methods.push_back(token);
			i++;
			continue ;
		}
		if (token == "}" || token == "root" || token == "upload_enable" || token == "upload_path" || token == "autoindex" ||
			token == "cgi_enabled" || token == "cgi_ext" || token == "cgi_extensions" || token == "return" ||
			token == "methods" || token == "allow_methods")
			throw ConfigParser::parseError(tokens[i].line, "expected ';'");
		throw ConfigParser::parseError(tokens[i].line, "invalid method '" + token + "' (expected GET/POST/DELETE)");
	}
	ConfigParser::expectToken(tokens, i, ";");
	if (methods.empty())
		throw ConfigParser::parseError(tokens[i - 1].line, "methods requires at least one method");
	loc.setAllowedMethods(methods);
}

void	parseRoot(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix)
{
	if (i >= tokens.size())
		throw ConfigParser::parseError(tokens[i - 1].line, "missing location root");
	loc.setRoot(applyPrefixPath(prefix, tokens[i++].value));
	ConfigParser::expectToken(tokens, i, ";");
}

void	parseUploadEnable(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix)
{
	(void) prefix;
	if (i >= tokens.size())
		throw ConfigParser::parseError(tokens[i - 1].line, "missing upload_enable value");
	loc.setUploadEnabled(parseValidOnOff(tokens[i].value, tokens[i - 1].line, "upload_enable"));
	i++;
	ConfigParser::expectToken(tokens, i, ";");
}

void	parseUploadPath(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix)
{
	if (i >= tokens.size())
		throw ConfigParser::parseError(tokens[i - 1].line, "missing upload_path value");
	loc.setUploadPath(applyPrefixPath(prefix, tokens[i++].value));
	ConfigParser::expectToken(tokens, i, ";");
}

void	parseAutoindex(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix)
{
	(void) prefix;
	if (i >= tokens.size())
		throw ConfigParser::parseError(tokens[i - 1].line, "missing autoindex value");
	loc.setAutoindex(parseValidOnOff(tokens[i].value, tokens[i - 1].line, "autoindex"));
	i++;
	ConfigParser::expectToken(tokens, i, ";");
}

void	parseCgiEnabled(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix)
{
	(void) prefix;
	if (i >= tokens.size())
		throw ConfigParser::parseError(tokens[i - 1].line, "missing cgi_enabled value");
	loc.setCgiEnabled(parseValidOnOff(tokens[i].value, tokens[i - 1].line, "cgi_enabled"));
	i++;
	ConfigParser::expectToken(tokens, i, ";");
}

void	parseCgiExtensions(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix)
{
	(void) prefix;
	if (i >= tokens.size())
		throw ConfigParser::parseError(tokens[i - 1].line, "missing cgi_ext values");
	std::vector<std::string> extensions;
	while (i < tokens.size() && tokens[i].value != ";")
	{
		const std::string&	ext = tokens[i++].value;
		if (ext.size() < 2 || ext[0] != '.' || ext.find('/') != std::string::npos)
			throw ConfigParser::parseError(tokens[i - 1].line, "invalid cgi_ext extension: " + ext);
		if (std::find(extensions.begin(), extensions.end(), ext) != extensions.end())
			throw ConfigParser::parseError(tokens[i - 1].line, "duplicate cgi_ext extension: " + ext);
		extensions.push_back(ext);
	}
	ConfigParser::expectToken(tokens, i, ";");
	if (extensions.empty())
		throw ConfigParser::parseError(tokens[i - 1].line, "cgi_ext requires at least one extension");
	loc.setCgiExtensions(extensions);
}

void	parseReturn(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix)
{
	(void) prefix;
	if (i >= tokens.size())
		throw ConfigParser::parseError(tokens[i - 1].line, "missing return status code");
	unsigned long	codeUl = parseUnsigned(tokens[i].value, "return", tokens[i].line);
	i++;
	if (i >= tokens.size() || tokens[i].value == ";")
		throw ConfigParser::parseError(tokens[i - 1].line, "missing return target");
	std::string	target = tokens[i++].value;
	ConfigParser::expectToken(tokens, i, ";");
	if (codeUl > 999)
		throw ConfigParser::parseError(tokens[i - 1].line, "invalid return status code");
	int	code = static_cast<int>(codeUl);
	if (!isSupportedReturnStatus(code))
	{
		std::ostringstream	oss;
		oss << "unsupported return status code: " << code;
		throw ConfigParser::parseError(tokens[i - 1].line, oss.str());
	}
	loc.setReturnDirective(true, code, target);
}

static void	parseLocationBlock(const std::vector<ConfigToken>& tokens, size_t& i, ServerConfig& server, const std::string& prefix)
{
	if (i >= tokens.size())
		throw ConfigParser::parseError(tokens[tokens.size() - 1].line, "missing location path");
	LocationConfig loc;
	loc.setPath(tokens[i++].value);
	ConfigParser::expectToken(tokens, i, "{");
	while (i < tokens.size() && tokens[i].value != "}")
	{
		std::string key = tokens[i++].value;
		const std::map<std::string, LocationDirectiveHandler>&	handlers = ConfigParser::getLocationHandlers();
		if (handlers.count(key))
			handlers.find(key)->second(tokens, i, loc, prefix);
		else
			throw ConfigParser::parseError(tokens[i - 1].line, "unknown location directive '" + key + "'");
	}
	ConfigParser::expectToken(tokens, i, "}");
	server.addLocation(loc);
}

std::runtime_error	ConfigParser::parseError(size_t line, const std::string& message)
{
	std::ostringstream oss;
	oss << "Config parse error at line " << line << ": " << message;
	return std::runtime_error(oss.str());
}

std::string	ConfigParser::trim(const std::string& s)
{
	size_t start = 0;
	while (start < s.size() && (s[start] == ' ' || s[start] == '\t' || s[start] == '\r' || s[start] == '\n'))
		++start;
	size_t end = s.size();
	while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r' || s[end - 1] == '\n'))
		--end;
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
		throw ConfigParser::parseError(line, "expected '" + expected + "'");
	}
	++i;
}

bool	ConfigParser::isDirectoryPath(const std::string& path)
{
	struct stat st;
	if (stat(path.c_str(), &st) != 0)
		return false;
	return S_ISDIR(st.st_mode);
}

std::vector<ConfigToken>	ConfigParser::tokenize(const std::string& content)
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

ServerConfig	ConfigParser::parseServerBlock(const std::vector<ConfigToken>& tokens, size_t& i, const std::string& prefix)
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

const std::map<std::string, LocationDirectiveHandler>& ConfigParser::getLocationHandlers(void)
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