/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   engine.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hwai-keo <hwai-keo@student.42kl.edu.my>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/19 20:53:37 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/02/26 18:03:12 by hwai-keo         ###   ########.fr       */
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
#include <sstream>

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

void Engine::registerListenSocketsForSelect(fd_set& readSet, int& maxFd)
{
	for (std::map<std::pair<std::string, int>, int>::const_iterator it = _listenSockets.begin();
		it != _listenSockets.end(); ++it)
	{
		//std::cout << "tracing listen fd=" << it->second << std::endl;
		int fd = it->second;
		FD_SET(fd, &readSet);
		if (fd > maxFd)
			maxFd = fd;
	}
}

void Engine::registerClientSocketForSelect(fd_set& readSet, fd_set& writeSet, int& maxFd)
{
	for (std::map<int, Connection*>::const_iterator it = _connections.begin();
		it != _connections.end(); ++it)
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

void Engine::acceptPendingClientConnections(fd_set& readSet)
{
	for (std::map<std::pair<std::string, int>, int>::const_iterator it = _listenSockets.begin();
		it != _listenSockets.end(); ++it)
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
				_connections[clientFd] = new Connection(clientFd);
			}
		}
	}
}

bool Engine::parseRequestLine(Connection* conn)
{
	std::string& buffer = conn->getReadBuffer();
	size_t lineEnd = buffer.find("\r\n");
	if (lineEnd == std::string::npos)
		return false;
	std::string requestLine = buffer.substr(0, lineEnd);
	std::istringstream iss(requestLine);
	std::string method, path, version;

	if (!(iss >> method >> path >> version))
		return false;
	std::string extra;
	if (iss >> extra)
		return false;
	if (path.empty() || path[0] != '/')
		return false;
	if (version.find("HTTP/") != 0)
		return false;
	return true;
}

void Engine::handleClientRequest(Connection* conn, const char* buffer, ssize_t bytes)
{
	std::cout << "State before detection: " << conn->getState() << std::endl;
	conn->appendToReadBuffer(buffer, bytes);
	if (conn->getState() == Connection::READING)
	{
		if (conn->headerComplete())
		{
			std::cout << "Header complete\n";
			if (parseRequestLine(conn))
			{
				std::string& buffer = conn->getReadBuffer();
				size_t headerEnd = buffer.find("\r\n\r\n");
				size_t bodyStart = headerEnd + 4; //new
				size_t currentBodySize = buffer.length() - bodyStart; //new
				std::string headerSection = buffer.substr(0, headerEnd);
				
				std::istringstream stream(headerSection);
				std::string line;
				bool firstLine = true;

				std::map<std::string, std::string>headers;
				while (std::getline(stream, line))
				{
					if (!line.empty() && line[line.length() - 1] == '\r')
						line.erase(line.length() - 1);
					if (firstLine)
					{
						firstLine = false;
						continue;
					}
					size_t colonPos = line.find(':');
					if (colonPos == std::string::npos)
					{
						conn->getWriteBuffer() = build400Response();
						conn->setState(Connection::WRITING);
						return; 
					}
					std::string key = line.substr(0, colonPos);
					std::string value = line.substr(colonPos + 1);
					
					if (!value.empty() && value[0] == ' ')
						value.erase(0, 1);
					headers[key] = value;
				}
				std::map<std::string, std::string>::iterator it = headers.find("Content-Length");
				if (it != headers.end())
				{
					std::string value = it->second;
					if (value.empty())
					{
						conn->getWriteBuffer() = build400Response();
						conn->setState(Connection::WRITING);
						return;
					}
					for (size_t i = 0; i < value.length(); ++i)
					{
						if (!isdigit(value[i]))
						{
							conn->getWriteBuffer() = build400Response();
							conn->setState(Connection::WRITING);
							return;
						}
					}
					size_t expectBodySize = std::atoi(value.c_str()); //new
					if (currentBodySize < expectBodySize) // newblock
					{
						return;
					}	
				}

				std::cout << "Request line valid\n";
				conn->getWriteBuffer() = buildMinimalResponse();
			}
			else
			{	
				std::cout << "Request line invalid\n";
				conn->getWriteBuffer() = build400Response();
			}
			conn->setState(Connection::WRITING);
			std::cout << "Switchhed to writting\n";
		}
	}
	//conn->getWriteBuffer() = buildMinimalResponse();
	//conn->setState(Connection::WRITING);
}

void Engine::processIncomingData(fd_set& readSet)
{
	for (std::map<int, Connection*>::iterator it = _connections.begin();
			it != _connections.end();)
	{
		int clientFd = it->first;
		if (FD_ISSET(clientFd, &readSet))
		{
			char buffer[1024];
			ssize_t bytes = recv(clientFd, buffer, sizeof(buffer), 0);
			std::cout << "recv returned: " << bytes << std::endl;
			if (bytes > 0)
			{
				handleClientRequest(it->second, buffer, bytes);
				continue;
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
}

void Engine::processOutgoingData(fd_set& writeSet)
{
	for (std::map<int, Connection*>::iterator it = _connections.begin();
			it != _connections.end();)
	{
		int clientFd = it->first;
		if (FD_ISSET(clientFd, &writeSet))
		{
			std::string& writebuffer = it->second->getWriteBuffer();
			if (!writebuffer.empty())
			{
				std::cout << "about to send on fd=" << clientFd << std::endl;
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
					_connections.erase(it++);
					continue;
				}
				if (writebuffer.empty())
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

		registerListenSocketsForSelect(readSet, maxFd);
		registerClientSocketForSelect(readSet, writeSet, maxFd);
		int readyFdCount = select(maxFd + 1, &readSet, &writeSet, NULL, NULL);
		std::cout << "select return= " << readyFdCount << std::endl;
		if (readyFdCount < 0)
		{
			if (errno == EINTR)
				continue;
			perror("select");
			break;
		}
		acceptPendingClientConnections(readSet);
		processIncomingData(readSet);
		processOutgoingData(writeSet);
	}
}

std::string Engine::build400Response()
{
	return "HTTP/1.1 400 Bad Request\r\n"
		"Content-Length: 11\r\n"
		"Content-Type: text/plain\r\n"
		"\r\n"
		"Bad Request";
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
