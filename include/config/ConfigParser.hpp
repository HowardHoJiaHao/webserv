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

class ConfigParser
{
	public:
		struct ConfigToken
		{
			std::string	value;
			size_t		line;

			ConfigToken(int l, const std::string& v);
			~ConfigToken(void);
		};

		static std::string				trim(const std::string& s);
		static void						expectToken(const std::vector<ConfigToken>& tokens, size_t& i, const std::string& expected);
		static bool						isDirectoryPath(const std::string& path);
		static std::vector<ConfigToken>	tokenize(const std::string& content);
		static std::runtime_error		parseError(size_t line, const std::string& message);
		static ServerConfig				parseServerBlock(const std::vector<ConfigToken>& tokens, size_t& i, const std::string& prefix);

	private:
		typedef void				(*LocationDirectiveHandler)(const std::vector<ConfigToken>&, size_t&, LocationConfig&, const std::string&);
		typedef void				(*ServerDirectiveHandler)(const std::vector<ConfigToken>&, size_t&, ServerConfig&, const std::string&);
		typedef const std::string&	(*Validator)(const ConfigToken&);

		static void					flushToken(std::vector<ConfigToken>& tokens, std::string& current, size_t line);
		static bool					isValidIpv4OrLocalhost(const std::string& host);
		static bool					isValidPort(const std::string& portstr, int& port);
		static const ConfigToken&	nextTokenOrError(const std::vector<ConfigToken>& tokens, size_t& i, const std::string& context);
		static const ConfigToken&	expectValueToken(const std::vector<ConfigToken>& tokens, size_t& i, const std::string& context);
		static std::string			requireHost(const ConfigToken& token);
		static int					requirePort(const ConfigToken& token);
		static size_t				requireNumericValue(const ConfigToken& token);
		static int					requireNumericValue(const ConfigToken& token, const std::string& context);
		static std::string			requireErrorPageTarget(const std::vector<ConfigToken>& tokens, size_t& i, const std::string& prefix);
		static std::string			applyPrefixPath(const std::string& prefix, const std::string& path);
		static int					requireReturnCode(const ConfigToken& token);
		static std::string			requireReturnTarget(const std::vector<ConfigToken>& tokens, size_t& i);
		static void					applyServerDefaults(ServerConfig& server, const std::string& prefix);
		static void					ensureUnique(const std::vector<std::string>& values, const ConfigToken& token, const std::string& directive);
		static const std::string&	requireMethod(const ConfigToken& token);
		static const std::string&	requireExtension(const ConfigToken& token);
		static const std::string&	expectValueWithValidator(const std::vector<ConfigToken>& tokens, size_t& i, const std::string& directive, Validator validator);
		static std::runtime_error	parseError(size_t line, const std::string& message, int code);
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
		static void					parsePort(const std::vector<ConfigToken>& tokens, size_t& i, ServerConfig& server, const std::string& prefix);
		static void					parseRoot(const std::vector<ConfigToken>& tokens, size_t& i, ServerConfig& server, const std::string& prefix);
		static void					parseIndex(const std::vector<ConfigToken>& tokens, size_t& i, ServerConfig& server, const std::string& prefix);
		static void					parseClientMaxBodySize(const std::vector<ConfigToken>& tokens, size_t& i, ServerConfig& server, const std::string& prefix);
		static void					parseErrorPage(const std::vector<ConfigToken>& tokens, size_t& i, ServerConfig& server, const std::string& prefix);
		static void					parseLocationBlock(const std::vector<ConfigToken>& tokens, size_t& i, ServerConfig& server, const std::string& prefix);

		static const std::map<std::string, bool>&						getValidOnOffMap(void);
		static const std::map<std::string, LocationDirectiveHandler>&	getLocationHandlers(void);
		static const std::map<std::string, ServerDirectiveHandler>&		getServerHandlers(void);

};

#endif
