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

class ConfigParser
{
	public:
		static std::runtime_error		parseError(size_t line, const std::string& message);
		static std::string				trim(const std::string& s);
		static void						expectToken(const std::vector<ConfigToken>& tokens, size_t& i, const std::string& expected);
		static bool						isDirectoryPath(const std::string& path);
		static std::vector<ConfigToken>	tokenize(const std::string& content);
		static ServerConfig				parseServerBlock(const std::vector<ConfigToken>& tokens, size_t& i, const std::string& prefix);

		static const std::map<std::string, LocationDirectiveHandler>&	getLocationHandlers(void);
};

#endif
