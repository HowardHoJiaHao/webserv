/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigFiles.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hwai-keo <hwai-keo@student.42kl.edu.my>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/17 22:48:17 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/02/23 16:05:11 by hwai-keo         ###   ########.fr       */
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
	public:
		ConfigFiles();
		const std::vector<ServerConfig>& getServers()const;
};

#endif // CONFIGFILES_HPP