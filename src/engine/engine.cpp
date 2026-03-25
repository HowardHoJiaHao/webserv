/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   engine.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hwai-keo <hwai-keo@student.42kl.edu.my>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/19 20:53:37 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/03/25 18:23:29 by hwai-keo         ###   ########.fr       */
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
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include "httpHandling/httpRequest.hpp"
#include "FileHandler.hpp"
#include <sys/stat.h>


Engine::Engine(const ConfigFiles& config) : _config(config){}

//close fd, destructor
Engine::~Engine()
{
	for (std::map<int, Connection*>::iterator it = _clientConnections.begin();
		it != _clientConnections.end(); ++it)
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

// check if localhost and 127.0.0.1 since both are local
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

// pointer cannot be store, reassigned or managed inside a container, but pointer can
// pointer enable persistency and changing state and stored in map
void Engine::registerClientSocketForSelect(fd_set& readSet, fd_set& writeSet, int& maxFd)
{
	for (std::map<int, Connection*>::const_iterator it = _clientConnections.begin();
		it != _clientConnections.end(); ++it)
	{
		//std::cout << "tracking client fd=" << it->first << std::endl;
		int fd = it->first;
		// check if this fd is valid
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

// _listenSockets -> clientSocketConnection
void Engine::acceptPendingClientConnections(fd_set& readSet)
{
	for (std::map<std::pair<std::string, int>, int>::const_iterator it = _listenSockets.begin();
		it != _listenSockets.end(); ++it)
	{
		int listenFd = it->second;
		// is listenFd present in readSet, modified by select()
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
				_clientConnections[clientFd] = new Connection(clientFd);
			}
		}
	}
}



void Engine::handleClientRequest(Connection* conn, const char* buffer, ssize_t bytes)
{
	//std::cout << "State before detection: " << conn->getState() << std::endl;
	conn->appendToReadBuffer(buffer, bytes);
	HttpRequest request;
	try
	{
		request.parse(conn->getReadBuffer());
	}
	catch(const std::exception& e)
	{
		conn->getWriteBuffer() = buildResponse("400 Bad Request", "Bad Request", "text/plain");
		conn->setState(Connection::WRITING);
		return;
	}
	// size_t headerEnd = conn->getReadBuffer().find("\r\n\r\n");
	// size_t bodyStart = (headerEnd == std::string::npos) ? 0 : headerEnd + 4;
	// size_t currentBodySize = conn->getReadBuffer().length() - bodyStart;
	if (!request.isComplete())
		return;
	conn->getWriteBuffer() = routeRequest(request);
	conn->setState(Connection::WRITING);
}

void Engine::processIncomingData(fd_set& readSet)
{
	for (std::map<int, Connection*>::iterator it = _clientConnections.begin();
			it != _clientConnections.end();)
	{
		int clientFd = it->first;
		if (FD_ISSET(clientFd, &readSet))
		{
			char buffer[1024];
			ssize_t bytes = recv(clientFd, buffer, sizeof(buffer), 0);
			std::cout << "recv returned: " << bytes << std::endl;
			// i received the actual data
			// process it
			// stay at this iterator
			if (bytes > 0)
			{
				handleClientRequest(it->second, buffer, bytes);
				++it;
				continue;
			}
			else if (bytes == 0)
			{
				std::cout << "Client disconnected on fd=" << clientFd << std::endl;
				close(clientFd);
				delete it->second;
				_clientConnections.erase(it++);
			}
			else
			{
				if (errno == EAGAIN || errno == EWOULDBLOCK)
				{
					++it;
					continue;
				}
				perror("recv");
				close(clientFd);
				delete it->second;
				_clientConnections.erase(it++);
			}
		}
		else
			++it;
	}
}

void Engine::processOutgoingData(fd_set& writeSet)
{
	for (std::map<int, Connection*>::iterator it = _clientConnections.begin();
			it != _clientConnections.end();)
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
					_clientConnections.erase(it++);
					continue;
				}
				if (writebuffer.empty())
				{
					delete it->second;
					_clientConnections.erase(it++);
					continue;
				}
			}
		}
		++it;
	}
}

// fd_set is a box of switches indexed by fd number
// the program will sleep when it reach select function until at least one signal with readset or 
// writeset is ready

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
		// signal interrupts it (EINTR)
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

// std::string Engine::build400Response()
// {
// 	return "HTTP/1.1 400 Bad Request\r\n"
// 		"Content-Length: 11\r\n"
// 		"Content-Type: text/plain\r\n"
// 		"\r\n"
// 		"Bad Request";
// }

std::string Engine::routeRequest(const HttpRequest& request)
{
	if (request.getMethod() == "GET")
	{
		if (request.getPath().find("..") != std::string::npos)
			return buildResponse("403 Forbidden", "Forbidden", "text/plain");
		std::string path = FileHandler::resolvePath(request.getPath());
		struct stat s;
		if (stat(path.c_str(), &s) == 0 && S_ISDIR(s.st_mode))
			path += "/index.html";

		if (!FileHandler::fileExists(path))
			return build404Response();
		std::string content = FileHandler::readFile(path);
		std::string mime = FileHandler::getMimeType(path);
		return buildResponse("200 OK", content,mime);
	}
	if (request.getMethod() == "POST")
		return handlePost(request);
	return build405Response();
	// if (request.getMethod() != "GET")
	// 	return build405Response();

	// if (request.getPath().find("..") != std::string::npos)
	// 	return buildResponse("403 Forbidden", "Forbidden", "text/plain");
	// std::string path = FileHandler::resolvePath(request.getPath());

	// struct stat s;
	// if (stat(path.c_str(), &s) == 0 && S_ISDIR(s.st_mode))
	// {
	// 	path += "/index.html";
	// }

	// if (!FileHandler::fileExists(path))
	// 	return build404Response();
	// std::string content = FileHandler::readFile(path);
	// std::string mime = FileHandler::getMimeType(path);
	// std::stringstream ss;
	// ss << "HTTP/1.1 200 OK\r\n";
	// ss << "Content-Length: " << content.size() << "\r\n";
	// ss << "Content-Type: " << mime << "\r\n";
	// ss << "\r\n";
	// ss << content;

	// return buildResponse("200 OK", content, mime);
	// if (request.getPath() == "/")
	// 	return buildIndexResponse();
	// return build404Response();
}

std::string Engine::handlePost(const HttpRequest& request)
{
	if (!request.isComplete())
		return "";

	const std::string& body = request.getBody();

	std::string path = "./www/upload.txt";
	int fd = open(path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
	if (fd < 0)
		return buildResponse("500 Internal Server Error", "Open Failed", "text/plain");
	size_t total = 0;
	while (total < body.size())
	{
		ssize_t written = write(fd, body.data() + total, body.size() - total);
		if (written <= 0)
		{
			close(fd);
			return buildResponse("500 Internal Server Error", "Write failed", "text/plain");
		}
		total += written;
	}
	close(fd);
	return buildResponse("200 OK", "OK", "text/plain");
}

std::string Engine::buildResponse
(
	const std::string& status,
	const std::string& body,
	const std::string& contentType
)
{
	std::stringstream ss;
	ss << "HTTP/1.1 " << status << "\r\n";
    ss << "Content-Length: " << body.size() << "\r\n";
    ss << "Content-Type: " << contentType << "\r\n";
    ss << "\r\n";
    ss << body;
    return ss.str();
}

std::string Engine::build405Response()
{
	return buildResponse("405 Method Not Allowed", "Method Not Allowed", "text/plain");
}

std::string Engine::buildIndexResponse()
{
	return "HTTP/1.1 200 OK\r\n"
		"Content-Length: 13\r\n"
		"Content-Type: text/plain\r\n"
		"\r\n"
		"Hello, world!";
}

std::string Engine::build404Response()
{
	return buildResponse("404 Not Found", "Not Found", "text/plain");
}

