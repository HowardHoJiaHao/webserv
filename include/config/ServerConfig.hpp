/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerConfig.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: Ho Wai Keong <hwai_keo@student.42kl.edu    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/17 23:16:56 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/02/18 09:58:35 by Ho Wai Keon      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

#include <string>
#include <vector>
#include "LocationConfig.hpp"

class ServerConfig
{
	private:
		std::string _host;
		int			_port;
		std::string	_serverName;
		std::string	_root;
		std::string	_index;

		std::vector<LocationConfig>	_locations;

	public:
		ServerConfig();

		void	setHost(const std::string& host);
		void	setPort(int port);
		void	setServerName(const std::string& name);
		void	setRoot(const std::string& root);
		void	setIndex(const std::string& index);

		void	addLocation(const LocationConfig& location);

		const	std::string& getHost()const;
		int		getPort()const;
		const	std::string& getServerName()const;
		const	std::string& getRoot()const;
		const	std::string& getIndex()const;

		const	std::vector<LocationConfig>& getLocations()const;
};

#endif