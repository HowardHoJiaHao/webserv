/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerConfig.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ktiew <ktiew@student.42kl.edu.my>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/20 12:29:07 by ktiew             #+#    #+#             */
/*   Updated: 2026/02/21 23:54:27 by ktiew            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

#include "LocationConfig.hpp"
#include <string>
#include <vector>
#include <map>

class ServerConfig
{
	private:
		std::string					_host;
		int							_port;
		std::string					_root;
		std::string					_index;
		size_t						_maxBodySize;
		std::map<int, std::string>	_errorPages;
		std::vector<LocationConfig>	_locations;

	public:
		ServerConfig(void);
		~ServerConfig(void);

		void	setHost(const std::string& host);
		void	setPort(int port);
		void	setRoot(const std::string& root);
		void	setIndex(const std::string& index);
		void	setMaxBodySize(size_t size);
		void	addErrorPage(int code, const std::string& path);

		void	addLocation(const LocationConfig& location);

		const	std::string& getHost(void)const;
		int		getPort(void)const;
		const	std::string& getRoot(void)const;
		const	std::string& getIndex(void)const;
		size_t	getMaxBodySize(void) const;

		const	std::string*					getErrorPage(int code) const;
		const	std::vector<LocationConfig>&	getLocations(void)const;
};

#endif
