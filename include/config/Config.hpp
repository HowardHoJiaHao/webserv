/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   DummyConfig.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: Ho Wai Keong <hwai_keo@student.42kl.edu    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/17 22:48:17 by Ho Wai Keon       #+#    #+#             */
<<<<<<< HEAD
/*   Updated: 2026/02/18 09:26:33 by Ho Wai Keon      ###   ########.fr       */
=======
/*   Updated: 2026/02/18 09:20:15 by Ho Wai Keon      ###   ########.fr       */
>>>>>>> 3580c6c (Refactor configuration files and add new config classes for server setup)
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
		std::vector<ServerConfig> _servers;
		void						initDummy();
	public:
		Config();
		const std::vector<ServerConfig>& getServers()const;
};

#endif