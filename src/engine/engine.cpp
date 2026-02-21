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
#include <sys/select.h>
#include <sys/time.h>
#include <fcntl.h>

Engine::Engine(const Config& config) : _config(config){}

//close fd, destructor
Engine::~Engine()
{
	for (std::map<int, Connection*>::iterator it = _connections.begin();
		it != _connections.end(); ++it)
	{
		delete it->second;
	}
	for(std::map<std::pair<std::string, int>, int>::iterator it = _listenSockets.begin(); 
		it != _listenSockets.end(); ++it)
		close(it->second);
		//delete it->second;
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
	fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
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
		fd_set readSet;
		FD_ZERO(&readSet);
		fd_set writeSet;
		FD_ZERO(&writeSet);

		int maxFd = 0;

		for (std::map<std::pair<std::string, int>, int>::iterator it = _listenSockets.begin();
			it != _listenSockets.end(); ++it)
		{
			//std::cout << "tracing listen fd=" << it->second << std::endl;
			int fd = it->second;
			FD_SET(fd, &readSet);
			if (fd > maxFd)
				maxFd = fd;
		}

		for (std::map<int, Connection*>::iterator it = _connections.begin();
			it != _connections.end(); ++it)
		{
			//std::cout << "tracking client fd=" << it->first << std::endl;
			int fd = it->first;

			
			if (fcntl(fd, F_GETFD) == -1)
				perror("FD invalid"	);
			
			// FD_SET(fd, &readSet);
			// if (!it->second->getWriteBuffer().empty())
			// 	FD_SET(fd, &writeSet);
			if (it->second->getState() == Connection::READING)
				FD_SET(fd, &readSet);
			if (it->second->getState() == Connection::WRITING)
				FD_SET(fd,&writeSet);
			if (fd > maxFd)
				maxFd = fd;
		}
		int activity = select(maxFd + 1, &readSet, &writeSet, NULL, NULL);
		std::cout << "select return= " << activity << std::endl;
		if (activity < 0)
		{
			if (errno == EINTR)
				continue;
			perror("select");
			break;
		}

		for (std::map<std::pair<std::string, int>, int>::iterator it = _listenSockets.begin();
			it != _listenSockets.end(); ++it)
		{
			int listenFd = it->second;
			{
				if (FD_ISSET(listenFd, &readSet))
				{
					int clientFd = accept(listenFd, NULL, NULL);
					if (clientFd >= 0)
					{
						int flags = fcntl(clientFd, F_GETFL, 0);
						if (flags == -1)
						{
							close(clientFd);
							continue;
						}
						if (fcntl(clientFd, F_SETFL, flags | O_NONBLOCK) == -1)
						{
							close(clientFd);
							continue;
						}
						// _connections.insert(std::make_pair(clientFd, Connection(clientFd)));
						//std::cout << "Accepted fd = " << clientFd << std::endl;

						if (fcntl(clientFd, F_GETFD) == -1)
							perror("FD invalid immediately after accept");
						_connections[clientFd] = new Connection(clientFd);
						
						//std::cout << "New connection on fd=" << clientFd << std::endl;
					}

				}
			}
		}
	
		for (std::map<int, Connection*>::iterator it = _connections.begin();
			it != _connections.end();)
		{
			int clientFd = it->first;
			if (FD_ISSET(clientFd, &readSet))
			{
				char buffer[1024];
				int bytes = recv(clientFd, buffer, sizeof(buffer), 0);
				std::cout << "recv returned: " << bytes << std::endl;
				if (bytes > 0)
				{
					//it->second->getReadBuffer().append(buffer, bytes); 
					it->second->appendToReadBuffer(buffer, bytes); //new
					//std::string response = buildMinimalResponse(); //new
					// send(clientFd, response.c_str(), response.size(), 0);
					// delete it->second;
					// _connections.erase(it++);
					it->second->getWriteBuffer() = buildMinimalResponse();
					it->second->setState(Connection::WRITING);
					++it;
					continue; //new
					// std::cout << "Received " << bytes << 
					// " bytes from fd=" << clientFd << std::endl;
					// ++it; //old
				}
				else if (bytes == 0)
				{
					std::cout << "Client disconnected on fd=" << clientFd << std::endl;
					delete it->second;
					_connections.erase(it++);
				}
				else
				{
					if (errno == EAGAIN || errno == EWOULDBLOCK)
					{
						++it;
						continue;
					}
					perror("recv");
					delete it->second;
					_connections.erase(it++);
				}
			}
			else
				++it;
		}
		for (std::map<int, Connection*>::iterator it = _connections.begin();
			it != _connections.end();)
		{
			int clientFd = it->first;
			if (FD_ISSET(clientFd, &writeSet))
			{
				std::string& wb = it->second->getWriteBuffer();
				if (!wb.empty())
				{
					ssize_t sent = send(clientFd, wb.c_str(), wb.size(), 0);
					if (sent > 0)
						wb.erase(0, sent);
					else if (sent < 0)
					{
						if (errno == EAGAIN || errno == EWOULDBLOCK)
						{
							++it;
							continue;
						}
						perror("send");
						delete it->second;
						_connections.erase(it++);
						continue;
					}
					if (wb.empty())
					{
						delete it->second;
						_connections.erase(it++);
						continue;
					}
				}
			}
			++it;
		}
	}
}

std::string Engine::buildMinimalResponse()
{
	return "HTTP/1.1 200 OK\r\n"
		"Content-Length: 13\r\n"
		"Content-Type: text/plain\r\n"
		"Connection: close\r\n"
		"\r\n"
		"Hello, world!";

	// alternatively
	// std::string body(2000000, 'A');

	// std::ostringstream oss;
	// oss << "HTTP/1.1 200 OK\r\n";
	// oss << "Content-Length: " << body.size() << "\r\n";
	// oss << "Content-Type: text/plain\r\n";
	// oss << "Connection: close\r\n";
	// oss << "\r\n";
	// oss << body;
	// return oss.str();
}
