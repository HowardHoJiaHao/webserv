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

/*
ConfigParser (Beginner friendly):
This class reads the webserv config file and turns it into C++ objects.

If you're new to programming, the key idea is simple:
- The config file is just text.
- We read the text and split it into small pieces (words and symbols).
- Then we interpret those pieces and fill ServerConfig and LocationConfig.

Mini example of the supported config style:

	prefix ./;               # optional base directory for paths
	server {
		listen 127.0.0.1:8080;
		root ./www1;
		index index.html;

		location / {
			methods GET POST;
			autoindex off;
		}
	}

Glossary:
- token: a single word or symbol from the config (like "server" or "{")
- directive: a command in the config (like "listen" or "root")
- handler map: a lookup table (string -> function) to call the right parser
*/

/*
ConfigToken Constructor:
A token is one small piece of the config file (word or symbol) plus its line
number. Keeping the line helps produce friendly error messages.
*/
ConfigParser::ConfigToken::ConfigToken(int l, const std::string& v)
	: value(v),
	line(l)
{
}

/*
ConfigToken Destructor:
Nothing special to clean up.
*/
ConfigParser::ConfigToken::~ConfigToken(void)
{
}

/*
trim:
Removes spaces/newlines at the start and end of a string.
Used to treat an "empty" config path as "no file provided".
*/
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

/*
parseError:
Creates a readable exception message like:
	"Config parse error at line 12: expected ';'"
*/
std::runtime_error	ConfigParser::parseError(size_t line, const std::string& message)
{
	std::ostringstream	oss;

	oss << "Config parse error at line " << line << ": " << message;
	return std::runtime_error(oss.str());
}

/*
parseError (with code):
Helper for messages that want to include a number (example: status code).
Throws immediately.
*/
std::runtime_error	ConfigParser::parseError(size_t line, const std::string& message, int code)
{
	std::ostringstream	oss;

	oss << message << code;
	throw parseError(line, oss.str());
}

/*
expectToken:
Checks that the next token is exactly what we expect (like "{" or ";").
If not, throw a helpful error.
*/
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

/*
nextTokenOrError:
Gets the next token.
If there is no next token, it means the config ended too early, so we throw.
*/
const ConfigParser::ConfigToken&	ConfigParser::nextTokenOrError(const std::vector<ConfigToken>& tokens, size_t& i, const std::string& context)
{
	if (i >= tokens.size())
		throw parseError(tokens[i - 1].line, "missing " + context + " value");
	return tokens[i++];
}

/*
expectValueToken:
Gets the next token, but rejects special symbols like ';' '{' '}' as values.
This prevents mistakes like writing "root ;" (missing the path).
*/
const ConfigParser::ConfigToken&	ConfigParser::expectValueToken(const std::vector<ConfigToken>& tokens, size_t& i, const std::string& context)
{
	const ConfigToken&	token = nextTokenOrError(tokens, i, context);

	if (token.value == ";" || token.value == "{" || token.value == "}")
		throw parseError(token.line, "missing " + context + " value");
	return token;
}

/*
flushToken:
Helper for tokenize(): when we finish building a word, push it into the list.
*/
void	ConfigParser::flushToken(std::vector<ConfigToken>& tokens, std::string& current, size_t line)
{
	if (!current.empty())
	{
		tokens.push_back(ConfigToken(line, current));
		current.clear();
	}
}

/*
tokenize:
Splits the config text into tokens.

Rules (easy version):
- Spaces separate words
- '#' starts a comment until the end of the line
- '{', '}', ';' become their own tokens
- Each token remembers its line number
*/
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

/*
isDirectoryPath:
Returns true only if the path exists AND is a directory.
Used for validating the "prefix" directive.
*/
bool	ConfigParser::isDirectoryPath(const std::string& path)
{
	struct stat	st;

	if (stat(path.c_str(), &st) != 0)
		return false;
	return S_ISDIR(st.st_mode);
}

/*
isValidIpv4OrLocalhost:
Accepts "localhost" or an IPv4 address like 127.0.0.1.
*/
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

/*
isValidPort:
Parses a port like "8080" into an integer and checks it is 1..65535.
*/
bool	ConfigParser::isValidPort(const std::string& portstr, int& port)
{
	std::stringstream	ss(portstr);

	ss >> port;
	if (ss.fail() || !ss.eof())
		return false;
	return port > 0 && port < 65536;
}

/*
requireHost:
Validates a host token and returns it.
Throws if invalid.
*/
std::string	ConfigParser::requireHost(const ConfigToken& token)
{
	if (!isValidIpv4OrLocalhost(token.value))
		throw parseError(token.line, "invalid host: " + token.value);
	return token.value;
}

/*
requirePort:
Validates a port token and returns it as an int.
Throws if invalid.
*/
int	ConfigParser::requirePort(const ConfigToken& token)
{
	int	port;

	if (!isValidPort(token.value, port))
		throw parseError(token.line, "invalid port: " + token.value);
	return port;
}

/*
requireNumericValue (size_t):
Reads a positive number used by directives like client_max_body_size.
*/
size_t	ConfigParser::requireNumericValue(const ConfigToken& token)
{
	std::stringstream	ss(token.value);
	size_t				num;

	ss >> num;
	if (ss.fail() || !ss.eof())
		throw parseError(token.line, "invalid client max body size: " + token.value);
	return num;
}

/*
requireNumericValue (int + context):
Reads a number into an int and uses 'context' to build a nicer error message.
*/
int	ConfigParser::requireNumericValue(const ConfigToken& token, const std::string& context)
{
	std::stringstream	ss(token.value);
	int					code;

	ss >> code;
	if (ss.fail() || !ss.eof())
		throw parseError(token.line, "invalid " + context + ": " + token.value);
	return code;
}

/*
requireMethod:
Only allow methods supported by this project: GET, POST, DELETE.
*/
const std::string&	ConfigParser::requireMethod(const ConfigToken& token)
{
	if (token.value != "GET" && token.value != "POST" && token.value != "DELETE")
		throw parseError(token.line, "invalid method '" + token.value + "' (expected GET/POST/DELETE)");
	return token.value;
}

/*
requireExtension:
Validates CGI extensions like .py or .pl.
*/
const std::string&	ConfigParser::requireExtension(const ConfigToken& token)
{
	const std::string&	ext = token.value;

	if (ext.size() < 2 || ext[0] != '.' || ext.find('/') != std::string::npos)
		throw parseError(token.line, "invalid cgi_ext extension: " + ext);
	return ext;
}

/*
ensureUnique:
Prevents duplicates in lists (example: "methods GET GET;").
*/
void	ConfigParser::ensureUnique(const std::vector<std::string>& values, const ConfigToken& token, const std::string& directive)
{
	if (std::find(values.begin(), values.end(), token.value) != values.end())
		throw parseError(token.line, "duplicate " + directive + ": " + token.value);
}

/*
expectValueWithValidator:
Reads one value token and validates it using the provided function.
This avoids repeating the same checks in multiple directive parsers.
*/
const std::string&	ConfigParser::expectValueWithValidator(const std::vector<ConfigToken>& tokens, size_t& i, const std::string& directive, Validator validator)
{
	const ConfigToken&	token = expectValueToken(tokens, i, directive);

	return validator(token);
}

/*
parseValidOnOff:
Parses a boolean setting from text:
- on/true/1 means true
- off/false/0 means false
*/
bool	ConfigParser::parseValidOnOff(const std::string& value, size_t line, const std::string& directive)
{
	const std::map<std::string, bool>&			map = getValidOnOffMap();
	std::map<std::string, bool>::const_iterator	it = map.find(value);

	if (it != map.end())
		return it->second;
	throw parseError(line, directive + " must be 'on/true/1' or 'off/false/0'");
}

/*
applyPrefixPath:
If the config has a prefix, add it in front of a path.
Example: prefix="/home/me/project" and root="./www" becomes
"/home/me/project/./www" (with clean slash handling).

If prefix is empty, return the path unchanged.
*/
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

/*
requireTarget:
Reads a generic "target" value for directives like error_page and return.
*/
std::string	ConfigParser::requireTarget(const std::vector<ConfigToken>& tokens, size_t& i, const std::string& directive)
{
	const ConfigToken&	token = expectValueToken(tokens, i, directive);

	return token.value;
}

/*
requireReturnCode:
Validates the status code used by "return".
This project supports redirect codes (300..399).
*/
int	ConfigParser::requireReturnCode(const ConfigToken& token)
{
	int	code = requireNumericValue(token, "return");

	if (code < 300 || code > 399)
		throw parseError(token.line, "unsupported return status code: ", code);
	return code;
}

/*
applyServerDefaults:
If the config file did not set some server values, fill them with defaults.
This makes small configs easier for beginners.
*/
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

/*
getValidOnOffMap:
Stores the accepted words for booleans and their meaning.
*/
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

/*
parseMethods (inside location):
Reads something like: methods GET POST;
It keeps reading values until it sees ';'.
*/
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

/*
parseRoot (inside location):
Reads: root <path>;
Prefix is applied if configured.
*/
void	ConfigParser::parseRoot(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix)
{
	const ConfigToken&	token = expectValueToken(tokens, i, "root");
	loc.setRoot(applyPrefixPath(prefix, token.value));
	expectToken(tokens, i, ";");
}

/*
parseIndex (inside location):
Reads: index <file>;
*/
void	ConfigParser::parseIndex(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix)
{
	(void) prefix;
	const ConfigToken&	token = expectValueToken(tokens, i, "index");
	loc.setIndex(token.value);
	expectToken(tokens, i, ";");
}

/*
parseUploadEnabled (inside location):
Reads: upload_enabled on/off;
*/
void	ConfigParser::parseUploadEnabled(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix)
{
	(void) prefix;
	const ConfigToken&	token = expectValueToken(tokens, i, "upload_enabled");
	loc.setUploadEnabled(parseValidOnOff(token.value, token.line, "upload_enabled"));
	expectToken(tokens, i, ";");
}

/*
parseUploadPath (inside location):
Reads: upload_path <path>;
Prefix is applied if configured.
*/
void	ConfigParser::parseUploadPath(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix)
{
	const ConfigToken&	token = expectValueToken(tokens, i, "upload_path");
	loc.setUploadPath(applyPrefixPath(prefix, token.value));
	expectToken(tokens, i, ";");
}

/*
parseAutoindex (inside location):
Reads: autoindex on/off;
*/
void	ConfigParser::parseAutoindex(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix)
{
	(void) prefix;
	const ConfigToken&	token = expectValueToken(tokens, i, "autoindex");
	loc.setAutoindex(parseValidOnOff(token.value, token.line, "autoindex"));
	expectToken(tokens, i, ";");
}

/*
parseCgiEnabled (inside location):
Reads: cgi_enabled on/off;
*/
void	ConfigParser::parseCgiEnabled(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix)
{
	(void) prefix;
	const ConfigToken&	token = expectValueToken(tokens, i, "cgi_enabled");
	loc.setCgiEnabled(parseValidOnOff(token.value, token.line, "cgi_enabled"));
	expectToken(tokens, i, ";");
}

/*
parseCgiExtensions (inside location):
Reads: cgi_ext .py .pl;
*/
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

/*
parseReturn (inside location):
Reads a redirect rule:
	return 301 /somewhere;
*/
void	ConfigParser::parseReturn(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix)
{
	(void) prefix;
	const ConfigToken&	token = expectValueToken(tokens, i, "return");

	loc.setReturnDirective(true, requireReturnCode(token), requireTarget(tokens, i, "return target"));
	expectToken(tokens, i, ";");
}

/*
getLocationHandlers:
Creates (once) a table like:
	"methods" -> parseMethods
	"root"    -> parseRoot
so the parser can quickly find the right function for each directive.
*/
const std::map<std::string, ConfigParser::LocationDirectiveHandler>&	ConfigParser::getLocationHandlers(void)
{
	static std::map<std::string, LocationDirectiveHandler>	map;

	if (map.empty())
	{
		map["methods"]			= &parseMethods;
		map["allow_methods"]	= &parseMethods;
		map["root"]				= &parseRoot;
		map["index"]			= &parseIndex;
		map["upload_enabled"]	= &parseUploadEnabled;
		map["upload_path"]		= &parseUploadPath;
		map["autoindex"]		= &parseAutoindex;
		map["cgi_enabled"]		= &parseCgiEnabled;
		map["cgi_ext"]			= &parseCgiExtensions;
		map["cgi_extensions"]	= &parseCgiExtensions;
		map["return"]			= &parseReturn;
	}
	return map;
}

/*
parseListen (inside server):
Reads:
	listen 8080;
or
	listen 127.0.0.1:8080;
If host is omitted, it uses 0.0.0.0.
*/
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

/*
parseHost (inside server):
Reads: host <ip/localhost>;
*/
void ConfigParser::parseHost(const std::vector<ConfigToken>& tokens, size_t& i, ServerConfig& server, const std::string& prefix)
{
	(void) prefix;
	const ConfigToken&	token = expectValueToken(tokens, i, "host");

	server.setHost(requireHost(token));
	expectToken(tokens, i, ";");
}

/*
parsePort (inside server):
Reads: port <number>;
*/
void ConfigParser::parsePort(const std::vector<ConfigToken>& tokens, size_t& i, ServerConfig& server, const std::string& prefix)
{
	(void) prefix;
	const ConfigToken&	token = expectValueToken(tokens, i, "port");

	server.setPort(requirePort(token));
	expectToken(tokens, i, ";");
}

/*
parseRoot (inside server):
Reads: root <path>;
Prefix is applied if configured.
*/
void	ConfigParser::parseRoot(const std::vector<ConfigToken>& tokens, size_t& i, ServerConfig& server, const std::string& prefix)
{
	const ConfigToken&	token = expectValueToken(tokens, i, "root");

	server.setRoot(applyPrefixPath(prefix, token.value));
	expectToken(tokens, i, ";");
}

/*
parseIndex (inside server):
Reads: index <file>;
*/
void	ConfigParser::parseIndex(const std::vector<ConfigToken>& tokens, size_t& i, ServerConfig& server, const std::string& prefix)
{
	(void) prefix;
	const ConfigToken&	token = expectValueToken(tokens, i, "index");

	server.setIndex(token.value);
	expectToken(tokens, i, ";");
}

/*
parseClientMaxBodySize (inside server):
Reads: client_max_body_size <number>;
*/
void	ConfigParser::parseClientMaxBodySize(const std::vector<ConfigToken>& tokens, size_t& i, ServerConfig& server, const std::string& prefix)
{
	(void) prefix;
	const ConfigToken&	token = expectValueToken(tokens, i, "client_max_body_size");

	server.setMaxBodySize(requireNumericValue(token));
	expectToken(tokens, i, ";");
}

/*
parseErrorPage (inside server):
Reads: error_page 404 /404.html;
If the same code is set multiple times, the latest value replaces the older.
*/
void	ConfigParser::parseErrorPage(const std::vector<ConfigToken>& tokens, size_t& i, ServerConfig& server, const std::string& prefix)
{
	(void) prefix;
	const ConfigToken&	token = expectValueToken(tokens, i, "error_page");

	server.addErrorPage(requireNumericValue(token, "error_page"), requireTarget(tokens, i, "error_page target"));
	expectToken(tokens, i, ";");
}

/*
parseLocationBlock (inside server):
Reads a section like:
	location /Upload { ... }

Steps:
1) Create a LocationConfig
2) Read directives inside the braces
3) Add the finished LocationConfig into the current ServerConfig
*/
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

/*
getServerHandlers:
Like getLocationHandlers, but for "server { ... }" directives.
*/
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

/*
parseServerBlock:
Parses one full:
	server { ... }
block, producing a ServerConfig object.

If the config contains an unknown directive name, it throws a parse error.
*/
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

/*
requirePrefixTarget:
Ensures "prefix" is placed before any "server" blocks.
This makes the rule easy to explain: set the base directory first, then define
servers that use it.
*/
const ConfigParser::ConfigToken&	ConfigParser::requirePrefixTarget(const std::vector<ConfigToken>& tokens, size_t& i, ConfigFiles& config)
{
	if (!config.getServers().empty())
		throw parseError(tokens[i].line, "'prefix' must appear before any 'server' block");
	expectToken(tokens, i, "prefix");
	return expectValueToken(tokens, i, "prefix");
}

/*
parsePrefix (top-level):
Reads: prefix <directory>;
Then checks that the directory exists.
*/
void ConfigParser::parsePrefix(const std::vector<ConfigToken>& tokens, size_t& i, ConfigFiles& config)
{
	const ConfigToken&	token = requirePrefixTarget(tokens, i, config);

	config.setPrefix(token.value);
	expectToken(tokens, i, ";");
	if (!isDirectoryPath(config.getPrefix()))
		throw parseError(token.line, "prefix must be an existing directory: " + config.getPrefix());
}

/*
parseServerBlockWrapper (top-level):
Reads one server block and stores it into ConfigFiles.
*/
void ConfigParser::parseServerBlockWrapper(const std::vector<ConfigToken>& tokens, size_t& i, ConfigFiles& config)
{
	config.addServer(parseServerBlock(tokens, i, config.getPrefix()));
}

/*
getTopLevelHandlers:
Top-level commands are the ones not inside braces.
This returns a table that tells the parser what to do when it sees:
- prefix
- server
*/
const std::map<std::string, ConfigParser::TopLevelDirectiveHandler>&	ConfigParser::getTopLevelHandlers(void)
{
	static std::map<std::string, TopLevelDirectiveHandler>	handlers;
	if (handlers.empty())
	{
		handlers["prefix"] = &ConfigParser::parsePrefix;
		handlers["server"] = &ConfigParser::parseServerBlockWrapper;
	}
	return handlers;
}
