/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Config.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: Ho Wai Keong <hwai_keo@student.42kl.edu    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/17 22:48:17 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/02/19 09:45:23 by Ho Wai Keon      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <vector>
#include <string>
#include "ServerConfig.hpp"

class Config
{
	private:
		std::vector<ServerConfig> _serverConfigs;
		void						initDummy();
	public:
		Config();
		const std::vector<ServerConfig>& getServers()const;
};

#endif