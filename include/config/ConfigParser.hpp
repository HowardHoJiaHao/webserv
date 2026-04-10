/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParser.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ktiew <ktiew@student.42kl.edu.my>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/20 12:29:07 by ktiew             #+#    #+#             */
/*   Updated: 2026/02/21 23:54:27 by ktiew            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONFIGPARSER_HPP
#define CONFIGPARSER_HPP

#include <vector>
#include <string>
#include <stdexcept>

struct ConfigToken
{
	std::string	value;
	size_t		line;
};

typedef void	(*LocationDirectiveHandler)(const std::vector<ConfigToken>&, size_t&, LocationConfig&, const std::string&);
typedef void	(*ServerDirectiveHandler)(const std::vector<ConfigToken>&, size_t&, ServerConfig&, const std::string&);

class ConfigParser
{
	private:
		static bool					isSupportedReturnStatus(int code);
		static std::string			applyPrefixPath(const std::string& prefix, const std::string& path);
		static std::runtime_error	parseError(size_t line, const std::string& message, int code);
		static void					parseListenValue(const std::string& listenValue, ServerConfig& server, size_t line);
		static unsigned long		parseUnsigned(const std::string& value, const std::string& fieldName, size_t line);
		static bool					parseValidOnOff(const std::string& value, size_t line, const std::string& directive);

		static void					parseMethods(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix);
		static void					parseRoot(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix);
		static void					parseUploadEnable(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix);
		static void					parseUploadPath(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix);
		static void					parseAutoindex(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix);
		static void					parseCgiEnabled(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix);
		static void					parseCgiExtensions(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix);
		static void					parseReturn(const std::vector<ConfigToken>& tokens, size_t& i, LocationConfig& loc, const std::string& prefix);

		static void					parseListen(const std::vector<ConfigToken>& tokens, size_t& i, ServerConfig& server, const std::string& prefix);
		static void					parseHost(const std::vector<ConfigToken>& tokens, size_t& i, ServerConfig& server, const std::string& prefix);
		static void					parseRoot(const std::vector<ConfigToken>& tokens, size_t& i, ServerConfig& server, const std::string& prefix);
		static void					parseIndex(const std::vector<ConfigToken>& tokens, size_t& i, ServerConfig& server, const std::string& prefix);
		static void					parseClientMaxBodySize(const std::vector<ConfigToken>& tokens, size_t& i, ServerConfig& server, const std::string& prefix);
		static void					parseErrorPage(const std::vector<ConfigToken>& tokens, size_t& i, ServerConfig& server, const std::string& prefix);
		static void					parseLocationBlock(const std::vector<ConfigToken>& tokens, size_t& i, ServerConfig& server, const std::string& prefix);

		static const std::map<std::string, bool>&						getValidOnOffMap(void);
		static const std::map<std::string, LocationDirectiveHandler>&	getLocationHandlers(void);
		static const std::map<std::string, ServerDirectiveHandler>&		getServerHandlers(void);

	public:
		static std::string				trim(const std::string& s);
		static void						expectToken(const std::vector<ConfigToken>& tokens, size_t& i, const std::string& expected);
		static bool						isDirectoryPath(const std::string& path);
		static std::vector<ConfigToken>	tokenize(const std::string& content);
		static std::runtime_error		parseError(size_t line, const std::string& message);
		static ServerConfig				parseServerBlock(const std::vector<ConfigToken>& tokens, size_t& i, const std::string& prefix);

};

#endif
