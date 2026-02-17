/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   DummyConfig.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: Ho Wai Keong <hwai_keo@student.42kl.edu    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/17 22:48:17 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/02/17 23:12:01 by Ho Wai Keon      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef DUMMYCONFIG_HPP
#define DUMMYCONFIG_HPP

#include <vector>
#include <string>
#include "ServerConfig.hpp"

class DummyConfig
{
	private:
		std::vector<ServerConfig> _servers;
		void						initDummy();
	public:
		DummyConfig();
		const std::vector<ServerConfig>& getServers()const;
};

#endif