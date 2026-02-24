/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   socket_utils.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hwai-keo <hwai-keo@student.42kl.edu.my>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/24 17:20:00 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/02/24 17:21:02 by hwai-keo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "socket_utils.hpp"
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <fcntl.h>

struct addrinfo* setupAddrInfoQuery(const std::string& host, int port)
{
	struct addrinfo		addressQuery;
	struct addrinfo*	osRes;

	std::memset(&addressQuery, 0, sizeof(addressQuery));
	addressQuery.ai_family = AF_INET;
	addressQuery.ai_socktype = SOCK_STREAM;
	addressQuery.ai_flags = AI_PASSIVE;

	std::ostringstream oss;
	oss << port;
	std::string portStr = oss.str();

	if (getaddrinfo(host.c_str(), portStr.c_str(), &addressQuery, &osRes) != 0)
		throw std::runtime_error("getaddrinfo failed");
	return osRes;
}

int attemptBindingSocket(struct addrinfo* osRes)
{
	struct addrinfo* ptr;
	int sockfd = -1;

	for (ptr = osRes; ptr != NULL; ptr = ptr->ai_next)
	{
		sockfd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (sockfd < 0)
			continue;

		int opt = 1;
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
		{
			close(sockfd);
			sockfd = -1;
			continue;
		}
		if (bind(sockfd, ptr->ai_addr, ptr->ai_addrlen) == 0)
			break;
		close(sockfd);
		sockfd = -1;
	}
	return sockfd;
}

void configureListeningNonblockingSocket(int& sockfd)
{
	if (listen(sockfd, SOMAXCONN) < 0)
	{
		close(sockfd);
		throw std::runtime_error("listen failed");
	}
	int flags = fcntl(sockfd, F_GETFL, 0);
	if (flags == -1)
	{
		close(sockfd);
		throw std::runtime_error("fcntl F_GETFL failed");
	}
	if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1)
	{
		close(sockfd);
		throw std::runtime_error("fcntl F_SETFL failed");
	}
}

// osRes is a linked list, but in this project it will always return single node
// ai_family -> ipv4
// socktype -> tcp style
// ai_flag -> become passive and waiting for connection
int createListeningSocket(const std::string& host, int port)
{
	struct addrinfo* osRes = setupAddrInfoQuery(host, port);
	int sockfd = attemptBindingSocket(osRes);
	freeaddrinfo(osRes);
	if (sockfd < 0)
		throw std::runtime_error("bind failed");
	configureListeningNonblockingSocket(sockfd);
	return sockfd;
}
