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

class ConfigFiles
{
	private:
		std::vector<ServerConfig>	_serverConfigs;
		std::string					_prefix;

		void	initDefault(void);
		void	loadFromFile(const std::string& path);

	public:
		ConfigFiles(void);
		ConfigFiles(const std::string& path);
		~ConfigFiles(void);

		const std::vector<ServerConfig>&	getServers(void) const;
		const std::string&					getPrefix(void) const;
};

#endif
