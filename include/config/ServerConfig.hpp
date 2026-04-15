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

/*
ServerConfig represents one `server { ... }` block from the config file.

Beginner idea:

- Think of a server block as "one website setup".
  It tells webserv: what address to listen on, where the web files are, and
  which special rules (locations) exist.

It keeps track of:

- Host + port (example: 127.0.0.1:8080).
- Default root folder and index file (used when a location does not override).
- Maximum request body size (to limit big uploads).
- Error pages (example: if 404 happens, serve a custom HTML file).
- A list of LocationConfig rules for different URL paths.

Typical runtime usage:

1) webserv listens on the configured host/port.
2) When a request arrives, it chooses the correct ServerConfig.
3) Then it chooses the best matching LocationConfig (if any).
4) Finally, the engine uses these settings to serve files / run CGI / upload.

This class does not do the work itself — it only stores the settings.
*/
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

		const std::string&					getHost(void)const;
		int									getPort(void)const;
		const std::string&					getRoot(void)const;
		const std::string&					getIndex(void)const;
		size_t								getMaxBodySize(void) const;
		const std::string*					getErrorPage(int code) const;
		const std::vector<LocationConfig>&	getLocations(void)const;
};

#endif
