/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   engine.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: Ho Wai Keong <hwai_keo@student.42kl.edu    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/19 20:53:37 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/02/19 21:58:12 by Ho Wai Keon      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "engine.hpp"
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <signal.h>

Engine::Engine(const Config& config) : _config(config){}

//close fd, destructor
Engine::~Engine()
{
	for(std::map<std::pair<std::string, int>, int>::iterator it = _listenSockets.begin(); 
		it != _listenSockets.end(); ++it)
		close(it->second);
}

static int createListeningSocket(const std::string& host, int port)
{
	struct addrinfo hints;
	struct addrinfo* res;
	struct addrinfo* p;

	std::memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	std::ostringstream oss;
	oss << port;
	std::string portStr = oss.str();

	if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0)
		throw std::runtime_error("getaddrinfo failed");

	int sockfd = -1;

	for (p = res; p != NULL; p = p->ai_next)
	{
		sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (sockfd < 0)
			continue;

		int opt = 1;
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
		{
			close(sockfd);
			sockfd = -1;
			continue;
		}
		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == 0)
			break;
		close(sockfd);
		sockfd = -1;
	}
	freeaddrinfo(res);
	if (sockfd < 0)
		throw std::runtime_error("bind failed");
	if (listen(sockfd, SOMAXCONN) < 0)
	{
		close(sockfd);
		throw std::runtime_error("listen failed");
	}
	return sockfd;
}

void Engine::setupListeningSockets()
{
	const std::vector<ServerConfig>& servers = _config.getServers();
	for (std::vector<ServerConfig>::const_iterator it = servers.begin(); it != servers.end(); ++it)
	{
		std::pair<std::string, int> key(it->getHost(), it->getPort());
		if (_listenSockets.find(key) == _listenSockets.end())
		{
			int fd = createListeningSocket(it->getHost(), it->getPort());
			_listenSockets[key] = fd;

			std::cout << "Listening on " << it->getHost() << ":"
					<< it->getPort() << " (fd=" << fd << ")"
					<< std::endl;
		}
	}
	if (_listenSockets.empty())
		throw std::runtime_error("No listening sockets created");
}

void Engine::run()
{
	std::cout << "Server running..." << std::endl;
	while (true)
	{
		pause();
	}
}