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
#include <sys/stat.h>

ConfigParser::ConfigToken::ConfigToken(int l, const std::string& v)
	: value(v),
	line(l)
{
}

ConfigParser::ConfigToken::~ConfigToken(void)
{
}

void	ConfigParser::flushToken(std::vector<ConfigToken>& tokens, std::string& current, size_t line)
{
	if (!current.empty())
	{
		tokens.push_back(ConfigToken(line, current));
		current.clear();
	}
}

bool	ConfigParser::isValidIpv4OrLocalhost(const std::string& host)
{
	if (host == "localhost")
		return true;
	std::istringstream	iss(host);
	std::string			token;
	int					parts = 0;

	while (std::getline(iss, token, '.'))
	{
		if (token.empty() || token.size() > 3)
			return false;
		std::stringstream	ss(token);
		int					num;

		ss >> num;
		if (ss.fail() || !ss.eof())
			return false;
		if (num < 0 || num > 255)
			return false;
		parts++;
	}
	return parts == 4;
}

bool	ConfigParser::isValidPort(const std::string& portstr, int& port)
{
	std::stringstream	ss(portstr);

	ss >> port;
	if (ss.fail() || !ss.eof())
		return false;
	return port > 0 && port < 65535;
}

const ConfigParser::ConfigToken&	ConfigParser::nextTokenOrError(const std::vector<ConfigToken>& tokens, size_t& i, const std::string& context)
{
	if (i >= tokens.size())
		throw parseError(tokens[i - 1].line, "missing " + context + " value");
	return tokens[i++];
}

const ConfigParser::ConfigToken&	ConfigParser::expectValueToken(const std::vector<ConfigToken>& tokens, size_t& i, const std::string& context)
{
	const ConfigToken&	token = nextTokenOrError(tokens, i, context);

	if (token.value == ";" || token.value == "{" || token.value == "}")
		throw parseError(token.line, "missing " + context + " value");
	return token;
}

std::string	ConfigParser::requireHost(const ConfigToken& token)
{
	if (!isValidIpv4OrLocalhost(token.value))
		throw parseError(token.line, "invalid host: " + token.value);
	return token.value;
}

int	ConfigParser::requirePort(const ConfigToken& token)
{
	int	port;

	if (!isValidPort(token.value, port))
		throw parseError(token.line, "invalid port: " + token.value);
	return port;
}

size_t	ConfigParser::requireNumericValue(const ConfigToken& token)
{
	std::stringstream	ss(token.value);
	size_t				num;

	ss >> num;
	if (ss.fail() || !ss.eof())
		throw parseError(token.line, "invalid client max body size: " + token.value);
	return num;
}

int	ConfigParser::requireNumericValue(const ConfigToken& token, const std::string& context)
{
	std::stringstream	ss(token.value);
	int					code;

	ss >> code;
	if (ss.fail() || !ss.eof())
		throw parseError(token.line, "invalid " + context + ": " + token.value);
	return code;
}

std::string	ConfigParser::requireErrorPageTarget(const std::vector<ConfigToken>& tokens, size_t& i, const std::string& prefix)
{
	const ConfigToken&	token = expectValueToken(tokens, i, "error_page target");

	return applyPrefixPath(prefix, token.value);
}

int	ConfigParser::requireReturnCode(const ConfigToken& token)
{
	int	code = requireNumericValue(token, "return");

	if (code < 300 || code > 399)
		throw parseError(token.line, "unsupported return status code: ", code);
	return code;
}

std::string	ConfigParser::requireReturnTarget(const std::vector<ConfigToken>& tokens, size_t& i)
{
	const ConfigToken&	token = expectValueToken(tokens, i, "return target");

	return token.value;
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

void	ConfigParser::applyServerDefaults(ServerConfig& server, const std::string& prefix)
{
	if (server.getHost().empty())
		server.setHost("0.0.0.0");
	if (server.getPort() == 0)
		server.setPort(1024);
	if (server.getRoot().empty())
		server.setRoot(applyPrefixPath(prefix, "./www"));
	if (server.getIndex().empty())
		server.setIndex("index.html");
}

void	ConfigParser::ensureUnique(const std::vector<std::string>& values, const ConfigToken& token, const std::string& directive)
{
	if (std::find(values.begin(), values.end(), token.value) != values.end())
		throw parseError(token.line, "duplicate " + directive + ": " + token.value);
}

const std::string&	ConfigParser::requireMethod(const ConfigToken& token)
{
	if (token.value != "GET" && token.value != "POST" && token.value != "DELETE")
		throw parseError(token.line, "invalid method '" + token.value + "' (expected GET/POST/DELETE)");
	return token.value;
}

const std::string&	ConfigParser::requireExtension(const ConfigToken& token)
{
	const std::string&	ext = token.value;

	if (ext.size() < 2 || ext[0] != '.' || ext.find('/') != std::string::npos)
		throw parseError(token.line, "invalid cgi_ext extension: " + ext);
	return ext;
}

const std::string&	ConfigParser::expectValueWithValidator(const std::vector<ConfigToken>& tokens, size_t& i, const std::string& directive, Validator validator)
{
	const ConfigToken&	token = expectValueToken(tokens, i, directive);

	return validator(token);
}

std::runtime_error	ConfigParser::parseError(size_t line, const std::string& message, int code)
{
	std::ostringstream	oss;

	oss << message << code;
	throw parseError(line, oss.str());
}

bool	ConfigParser::parseValidOnOff(const std::string& value, size_t line, const std::string& directive)
{
	const std::map<std::string, bool>&			map = getValidOnOffMap();
	std::map<std::string, bool>::const_iterator	it = map.find(value);

	if (it != map.end())
		return it->second;
	throw parseError(line, directive + " must be 'on/true/1' or 'off/false/0'");
}

void ConfigParser::parseMethods(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix)
{
	(void) prefix;
	std::vector<std::string> methods;

	while (i < tokens.size() && tokens[i].value != ";")
	{
		const std::string&	method = expectValueWithValidator(tokens, i, "methods", requireMethod);

		ensureUnique(methods, tokens[i - 1], "methods");
		methods.push_back(method);
	}
	expectToken(tokens, i, ";");
	if (methods.empty())
		throw parseError(tokens[i - 1].line, "methods requires at least one method");
	loc.setAllowedMethods(methods);
}

void	ConfigParser::parseRoot(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix)
{
	const ConfigToken&	token = expectValueToken(tokens, i, "root");
	loc.setRoot(applyPrefixPath(prefix, token.value));
	expectToken(tokens, i, ";");
}

void	ConfigParser::parseUploadEnable(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix)
{
	(void) prefix;
	const ConfigToken&	token = expectValueToken(tokens, i, "upload_enable");
	loc.setUploadEnabled(parseValidOnOff(token.value, token.line, "upload_enable"));
	expectToken(tokens, i, ";");
}

void	ConfigParser::parseUploadPath(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix)
{
	const ConfigToken&	token = expectValueToken(tokens, i, "upload_path");
	loc.setUploadPath(applyPrefixPath(prefix, token.value));
	expectToken(tokens, i, ";");
}

void	ConfigParser::parseAutoindex(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix)
{
	(void) prefix;
	const ConfigToken&	token = expectValueToken(tokens, i, "autoindex");
	loc.setAutoindex(parseValidOnOff(token.value, token.line, "autoindex"));
	expectToken(tokens, i, ";");
}

void	ConfigParser::parseCgiEnabled(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix)
{
	(void) prefix;
	const ConfigToken&	token = expectValueToken(tokens, i, "cgi_enabled");
	loc.setCgiEnabled(parseValidOnOff(token.value, token.line, "cgi_enabled"));
	expectToken(tokens, i, ";");
}

void	ConfigParser::parseCgiExtensions(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix)
{
	(void) prefix;
	std::vector<std::string>	extensions;

	while (i < tokens.size() && tokens[i].value != ";")
	{
		const std::string&	ext = expectValueWithValidator(tokens, i, "cgi_ext", requireExtension);

		ensureUnique(extensions, tokens[i - 1], "cgi_ext extension");
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
	const ConfigToken&	token = expectValueToken(tokens, i, "return");

	loc.setReturnDirective(true, requireReturnCode(token), requireReturnTarget(tokens, i));
	expectToken(tokens, i, ";");
}

void	ConfigParser::parseListen(const std::vector<ConfigToken>& tokens, size_t& i, ServerConfig& server, const std::string& prefix)
{
	(void) prefix;
	const ConfigToken&	token = expectValueToken(tokens, i, "listen");
	std::string			host;
	int					port;
	size_t				colon = token.value.rfind(':');

	if (colon == std::string::npos)
	{
		host = "0.0.0.0";
		port = requirePort(token);
	}
	else
	{
		ConfigToken	hostToken(token.line, token.value.substr(0, colon));

		if (hostToken.value.empty())
			host = "0.0.0.0";
		else
			host = requireHost(hostToken);
		ConfigToken	portToken(token.line, token.value.substr(colon + 1));

		port = requirePort(portToken);
	}
	server.setHost(host);
	server.setPort(port);
	expectToken(tokens, i, ";");
}

void ConfigParser::parseHost(const std::vector<ConfigToken>& tokens, size_t& i, ServerConfig& server, const std::string& prefix)
{
	(void) prefix;
	const ConfigToken&	token = expectValueToken(tokens, i, "host");

	server.setHost(requireHost(token));
	expectToken(tokens, i, ";");
}

void ConfigParser::parsePort(const std::vector<ConfigToken>& tokens, size_t& i, ServerConfig& server, const std::string& prefix)
{
	(void) prefix;
	const ConfigToken&	token = expectValueToken(tokens, i, "port");

	server.setPort(requirePort(token));
	expectToken(tokens, i, ";");
}

void	ConfigParser::parseRoot(const std::vector<ConfigToken>& tokens, size_t& i, ServerConfig& server, const std::string& prefix)
{
	const ConfigToken&	token = expectValueToken(tokens, i, "root");

	server.setRoot(applyPrefixPath(prefix, token.value));
	expectToken(tokens, i, ";");
}

void	ConfigParser::parseIndex(const std::vector<ConfigToken>& tokens, size_t& i, ServerConfig& server, const std::string& prefix)
{
	(void) prefix;
	const ConfigToken&	token = expectValueToken(tokens, i, "index");

	server.setIndex(token.value);
	expectToken(tokens, i, ";");
}

void	ConfigParser::parseClientMaxBodySize(const std::vector<ConfigToken>& tokens, size_t& i, ServerConfig& server, const std::string& prefix)
{
	(void) prefix;
	const ConfigToken&	token = expectValueToken(tokens, i, "client_max_body_size");

	server.setMaxBodySize(requireNumericValue(token));
	expectToken(tokens, i, ";");
}

void	ConfigParser::parseErrorPage(const std::vector<ConfigToken>& tokens, size_t& i, ServerConfig& server, const std::string& prefix)
{
	const ConfigToken&	token = expectValueToken(tokens, i, "error_page");

	server.addErrorPage(requireNumericValue(token, "error_page"), requireErrorPageTarget(tokens, i, prefix));
	expectToken(tokens, i, ";");
}

void	ConfigParser::parseLocationBlock(const std::vector<ConfigToken>& tokens, size_t& i, ServerConfig& server, const std::string& prefix)
{
	const ConfigToken&	token = expectValueToken(tokens, i, "location");
	LocationConfig		loc;

	loc.setPath(token.value);
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

const std::map<std::string, ConfigParser::LocationDirectiveHandler>&	ConfigParser::getLocationHandlers(void)
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

const std::map<std::string, ConfigParser::ServerDirectiveHandler>&	ConfigParser::getServerHandlers(void)
{
	static std::map<std::string, ServerDirectiveHandler>	map;

	if (map.empty())
	{
		map["listen"]				= &parseListen;
		map["host"]					= &parseHost;
		map["port"]					= &parsePort;
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
		size_t	line = 1;

		if (i < tokens.size())
			line = tokens[i].line;
		else if (!tokens.empty())
			line = tokens[tokens.size() - 1].line;
		throw parseError(line, "expected '" + expected + "'");
	}
	i++;
}

bool	ConfigParser::isDirectoryPath(const std::string& path)
{
	struct stat	st;

	if (stat(path.c_str(), &st) != 0)
		return false;
	return S_ISDIR(st.st_mode);
}

std::vector<ConfigParser::ConfigToken>	ConfigParser::tokenize(const std::string& content)
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
			flushToken(tokens, current, currentLine);
			continue;
		}
		if (c == '{' || c == '}' || c == ';')
		{
			flushToken(tokens, current, currentLine);
			tokens.push_back(ConfigToken(line, std::string(1, c)));
			continue;
		}
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
		{
			flushToken(tokens, current, currentLine);
			continue;
		}
		if (current.empty())
			currentLine = line;
		current += c;
	}
	flushToken(tokens, current, currentLine);
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
	applyServerDefaults(server, prefix);
	return server;
}
