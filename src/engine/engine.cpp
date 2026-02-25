/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   engine.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hwai-keo <hwai-keo@student.42kl.edu.my>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/19 20:53:37 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/02/25 18:22:10 by hwai-keo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "engine.hpp"
#include "socket_utils.hpp"
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <stdexcept>
#include <signal.h>
#include <sys/select.h>
#include <sys/time.h>
#include <fcntl.h>

Engine::Engine(const ConfigFiles& config) : _config(config){}

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



void Engine::setupListeningSockets()
{
	const std::vector<ServerConfig>& serverConfigs = this->_config.getServers();
	for (std::vector<ServerConfig>::const_iterator it = serverConfigs.begin(); it != serverConfigs.end(); ++it)
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

static void registerListenSocketsForSelect(const std::map<std::pair<std::string, int>
	, int>& listenSockets, fd_set& readSet, int& maxFd)
{
	for (std::map<std::pair<std::string, int>, int>::const_iterator it = listenSockets.begin();
		it != listenSockets.end(); ++it)
	{
		//std::cout << "tracing listen fd=" << it->second << std::endl;
		int fd = it->second;
		FD_SET(fd, &readSet);
		if (fd > maxFd)
			maxFd = fd;
	}
}

static void registerClientSocketForSelect(const std::map<int, Connection*>& connections,
	fd_set& readSet, fd_set& writeSet, int& maxFd)
{
	for (std::map<int, Connection*>::const_iterator it = connections.begin();
		it != connections.end(); ++it)
	{
		//std::cout << "tracking client fd=" << it->first << std::endl;
		int fd = it->first;

		if (fcntl(fd, F_GETFD) == -1)
		{
			perror("FD invalid");
			continue ;
		}
		if (it->second->getState() == Connection::READING)
			FD_SET(fd, &readSet);
		if (it->second->getState() == Connection::WRITING)
			FD_SET(fd, &writeSet);
		if (fd > maxFd)
			maxFd = fd;
	}
}

static void acceptPendingClientConnections(const std::map<std::pair<std::string, int>, int>& listenSockets,
	fd_set& readSet, std::map<int, Connection*>& connections)
{
	for (std::map<std::pair<std::string, int>, int>::const_iterator it = listenSockets.begin();
		it != listenSockets.end(); ++it)
	{
		int listenFd = it->second;
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
				connections[clientFd] = new Connection(clientFd);
			}
		}
	}
}

static void handleClientRequest(Connection* conn, const char* buffer, ssize_t bytes, Engine& engine)
{
	conn->appendToReadBuffer(buffer, bytes);
	conn->getWriteBuffer() = engine.buildMinimalResponse();
	conn->setState(Connection::WRITING);
}

static void processIncomingData(fd_set& readSet, std::map<int, Connection *>& connections
		, Engine& engine)
{
	for (std::map<int, Connection*>::iterator it = connections.begin();
			it != connections.end();)
	{
		int clientFd = it->first;
		if (FD_ISSET(clientFd, &readSet))
		{
			char buffer[1024];
			ssize_t bytes = recv(clientFd, buffer, sizeof(buffer), 0);
			std::cout << "recv returned: " << bytes << std::endl;
			if (bytes > 0)
			{
				handleClientRequest(it->second, buffer, bytes, engine);
				continue; //new
			}
			else if (bytes == 0)
			{
				std::cout << "Client disconnected on fd=" << clientFd << std::endl;
				delete it->second;
				connections.erase(it++);
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
				connections.erase(it++);
			}
		}
		else
			++it;
	}
}

static void processOutgoingData(fd_set& writeSet, std::map<int, Connection *>& connections)
{
	for (std::map<int, Connection*>::iterator it = connections.begin();
			it != connections.end();)
	{
		int clientFd = it->first;
		if (FD_ISSET(clientFd, &writeSet))
		{
			std::string& writebuffer = it->second->getWriteBuffer();
			if (!writebuffer.empty())
			{
				ssize_t sentByte = send(clientFd, writebuffer.c_str(), writebuffer.size(), 0);
				if (sentByte > 0)
					writebuffer.erase(0, sentByte);
				else if (sentByte < 0)
				{
					if (errno == EAGAIN || errno == EWOULDBLOCK)
					{
						++it;
						continue;
					}
					perror("send");
					delete it->second;
					connections.erase(it++);
					continue;
				}
				if (writebuffer.empty())
				{
					delete it->second;
					connections.erase(it++);
					continue;
				}
			}
		}
		++it;
	}
}

// fd_set is a box of switches indexed by fd number
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

		registerListenSocketsForSelect(_listenSockets, readSet, maxFd);
		registerClientSocketForSelect(_connections, readSet, writeSet, maxFd);
		int readyFdCount = select(maxFd + 1, &readSet, &writeSet, NULL, NULL);
		std::cout << "select return= " << readyFdCount << std::endl;
		if (readyFdCount < 0)
		{
			if (errno == EINTR)
				continue;
			perror("select");
			break;
		}
		acceptPendingClientConnections(_listenSockets, readSet, _connections);
		processIncomingData(readSet, _connections, *this);
		processOutgoingData(writeSet, _connections);
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
