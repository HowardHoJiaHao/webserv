/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigFiles.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ho <hwai-keo@student.42kl.edu.my>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/17 22:48:17 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/03/28 00:57:54 by ho               ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONFIGFILES_HPP
#define CONFIGFILES_HPP

#include <vector>
#include <string>
#include "ServerConfig.hpp"

class ConfigFiles
{
	private:
		std::vector<ServerConfig> _serverConfigs;
		void					initDummy();
		void					loadFromFile(const std::string& path);
	public:
		ConfigFiles();
		ConfigFiles(const std::string& path);
		const std::vector<ServerConfig>& getServers()const;
};

#endif // CONFIGFILES_HPP