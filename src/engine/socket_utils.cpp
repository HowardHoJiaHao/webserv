/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   socket_utils.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hwai-keo <hwai-keo@student.42kl.edu.my>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/24 17:20:00 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/04/08 17:29:26 by hwai-keo         ###   ########.fr       */
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
#include <cerrno>

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

	//create this osRes, whiuch is linked list of possible addresses
	if (getaddrinfo(host.c_str(), portStr.c_str(), &addressQuery, &osRes) != 0)
		throw std::runtime_error("getaddrinfo failed");
	return osRes;
}

// try each node until it works
int attemptBindingSocket(struct addrinfo* osRes, int& lastErrno)
{
	struct addrinfo* ptr;
	int sockfd = -1;
	lastErrno = 0;

	for (ptr = osRes; ptr != NULL; ptr = ptr->ai_next)
	{
		// create ipv4 tcp socket / standard tcp server socket
		sockfd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (sockfd < 0)
		{
			lastErrno = errno;
			continue;
		}
		//Sol_socket means, enable to modifying the socket
		int opt = 1; // turn on the resue address
		// ask kernel to allow this socket to reuse this address (ip:port), if restart happening
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
		{
			lastErrno = errno;
			close(sockfd);
			sockfd = -1;
			continue;
		}
		// bind this sockfd to ai_addr(address)
		if (bind(sockfd, ptr->ai_addr, ptr->ai_addrlen) == 0)
			break;
		lastErrno = errno;
		close(sockfd);
		sockfd = -1;
	}
	return sockfd;
}

void configureListeningNonblockingSocket(int& sockfd)
{
	// turn this socket to listening socket, ready to accpet incoming connection
	// somaxconn is max number of pending connection
	if (listen(sockfd, SOMAXCONN) < 0)
	{
		close(sockfd);
		throw std::runtime_error("listen failed");
	}
	if (fcntl(sockfd, F_SETFL, O_NONBLOCK) == -1)
	{
		close(sockfd);
		throw std::runtime_error("fcntl F_SETFL failed");
	}
}

// osResult is a linked list, but in this project it will always return single node
// ai_family -> ipv4
// socktype -> tcp style
// ai_flag -> become passive and waiting for connection
int createListeningSocket(const std::string& host, int port)
{
	struct addrinfo* osRes = setupAddrInfoQuery(host, port);
	int lastErrno = 0;
	int sockfd = attemptBindingSocket(osRes, lastErrno);
	// osRes is heap allocated linked list
	freeaddrinfo(osRes);
	if (sockfd < 0)
	{
		std::ostringstream oss;
		oss << "bind failed for " << host << ":" << port;
		if (lastErrno != 0)
			oss << ": " << std::strerror(lastErrno);
		throw std::runtime_error(oss.str());
	}
	configureListeningNonblockingSocket(sockfd);
	return sockfd;
}
