/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigFiles.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ktiew <ktiew@student.42kl.edu.my>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/20 12:29:07 by ktiew             #+#    #+#             */
/*   Updated: 2026/02/21 23:54:27 by ktiew            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONFIGFILES_HPP
#define CONFIGFILES_HPP

#include "ServerConfig.hpp"
#include <vector>
#include <string>

/*
ConfigFiles is the "settings book" for the whole webserv program.

When webserv starts, it reads a config file (like nginx-style config).
This class stores the result in an easy-to-use C++ form.

It keeps track of:

- A list of ServerConfig objects (each one is a `server { ... }` block).
- A global `prefix` (a base folder used to build full paths).

It supports:

- Starting with default settings (if no file is given).
- Loading settings from a config file path.
- Adding a server config manually (helpful for defaults/tests).

In simple terms:

- ConfigParser fills a ConfigFiles object.
- The engine reads ConfigFiles to know which ports to listen on and how to
  handle requests.
*/
class ConfigFiles
{
	private:
		std::vector<ServerConfig>	_serverConfigs;
		std::string					_prefix;

		static std::string	readFromFile(const std::string& path);

		void	initDefault(void);
		void	loadFromFile(const std::string& path);

	public:
		ConfigFiles(void);
		ConfigFiles(const std::string& path);
		~ConfigFiles(void);

		const std::vector<ServerConfig>&	getServers(void) const;
		const std::string&					getPrefix(void) const;
		void								setPrefix(const std::string& prefix);
		void								addServer(const ServerConfig& server);

};

#endif
