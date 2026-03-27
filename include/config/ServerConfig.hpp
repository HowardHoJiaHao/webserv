/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerConfig.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ho <hwai-keo@student.42kl.edu.my>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/17 23:16:56 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/03/28 00:34:17 by ho               ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

#include <string>
#include <vector>
#include <map>
#include "LocationConfig.hpp"

class ServerConfig
{
	private:
		std::string _host;
		int			_port;
		std::string	_serverName;
		std::string	_root;
		std::string	_index;
		size_t		_maxBodySize;
		std::map<int, std::string> _errorPages;

		std::vector<LocationConfig>	_locations;

	public:
		ServerConfig();

		void	setHost(const std::string& host);
		void	setPort(int port);
		void	setServerName(const std::string& name);
		void	setRoot(const std::string& root);
		void	setIndex(const std::string& index);
		void	setMaxBodySize(size_t size);
		void	addErrorPage(int code, const std::string& path);

		void	addLocation(const LocationConfig& location);

		const	std::string& getHost()const;
		int		getPort()const;
		const	std::string& getServerName()const;
		const	std::string& getRoot()const;
		const	std::string& getIndex()const;
		size_t	getMaxBodySize() const;
		const	std::string* getErrorPage(int code) const;

		const	std::vector<LocationConfig>& getLocations()const;
};

#endif