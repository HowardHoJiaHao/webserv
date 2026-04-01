/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   engine.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hho-jia- <hho-jia-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/19 20:53:37 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/04/01 16:44:35 by hho-jia-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "engine.hpp"
#include "engine_string_utils.hpp"
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
#include <cerrno>
#include <ctime>
#include <sys/wait.h>
#include <csignal>

static volatile sig_atomic_t g_engineStopRequested = 0;

static void handleEngineStopSignal(int)
{
	g_engineStopRequested = 1;
}

Engine::Engine(const ConfigFiles& config) : _config(config){}

//close fd, destructor
Engine::~Engine()
{
	for (std::map<int, Connection*>::iterator it = _clientConnections.begin();
		it != _clientConnections.end(); ++it)
	{
		Connection* conn = it->second;
		if (!conn)
			continue;
		const pid_t pid = conn->getCGIPid();
		if (pid > 0)
		{
			// Best-effort cleanup for CGI processes during shutdown.
			if (waitpid(pid, NULL, WNOHANG) == 0)
			{
				kill(pid, SIGTERM);
				waitpid(pid, NULL, WNOHANG);
				kill(pid, SIGKILL);
				waitpid(pid, NULL, WNOHANG);
			}
		}
	}
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
		if (it->second->getState() == Connection::READING)
			FD_SET(fd, &readSet);
		if (it->second->getState() == Connection::WRITING)
			FD_SET(fd, &writeSet);
		if (it->second->getState() == Connection::CGI_RUNNING)
		{
			Connection::CGIContext* cgi = it->second->getCGI();
			int cgi_fd = it->second->getCGIStdoutFd();
			if (cgi_fd != -1)
			{
				FD_SET(cgi_fd, &readSet);
				if (cgi_fd > maxFd)
					maxFd = cgi_fd;
			}
			if (cgi != NULL
				&& cgi->stdin_fd != -1
				&& !it->second->isCGIStdinClosed()
				&& cgi->stdin_offset < cgi->stdin_buffer.size())
			{
				FD_SET(cgi->stdin_fd, &writeSet);
				if (cgi->stdin_fd > maxFd)
					maxFd = cgi->stdin_fd;
			}
		}
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
				_clientListenEndpoints[clientFd] = it->first;
			}
		}
	}
}

const ServerConfig* Engine::findServerConfig(const std::string& host, int port) const
{
	const std::vector<ServerConfig>& servers = _config.getServers();
	for (size_t i = 0; i < servers.size(); ++i)
	{
		if (servers[i].getHost() == host && servers[i].getPort() == port)
			return &servers[i];
	}
	for (size_t i = 0; i < servers.size(); ++i)
	{
		if (servers[i].getPort() == port)
			return &servers[i];
	}
	if (servers.empty())
		return NULL;
	return &servers[0];
}

const ServerConfig* Engine::findServerConfigForConnection(int clientFd) const
{
	std::map<int, std::pair<std::string, int> >::const_iterator epIt = _clientListenEndpoints.find(clientFd);
	if (epIt == _clientListenEndpoints.end())
		return findServerConfig("", 0);

	std::string host = epIt->second.first;
	int port = epIt->second.second;
	return findServerConfig(host, port);
}

void Engine::processIncomingData(fd_set& readSet)
{
	for (std::map<int, Connection*>::iterator it = _clientConnections.begin();
			it != _clientConnections.end();)
	{
		int clientFd = it->first;

		Connection* conn = it->second;

		if (conn->getState() == Connection::CGI_RUNNING)
		{
			int cgi_fd = conn->getCGIStdoutFd();
			if (cgi_fd != -1 && FD_ISSET(cgi_fd, &readSet))
			{
				char buffer[1024];
				Connection::CGIContext* cgi = conn->getCGI();
				struct timeval start_time;
				gettimeofday(&start_time, NULL);
				while(true)
				{
					ssize_t bytes = read(cgi_fd, buffer, sizeof(buffer));
					if (bytes > 0)
					{
						if (cgi != NULL)
							cgi->stdout_buffer.append(buffer, bytes);
						conn->updateActivity();
					}
					else if (bytes == 0)
					{
						pid_t pid = conn->getCGIPid();
						if (pid > 0)
							waitpid(pid, NULL, WNOHANG);

						std::string cgiOutput;
						if (cgi != NULL)
							cgiOutput = cgi->stdout_buffer;

						if (cgiOutput.find("HTTP/1.") == 0)
						{
							conn->getWriteBuffer().append(cgiOutput);
						}
						else
						{
							std::string status = "200 OK";
							std::string contentType = "text/plain";
							std::vector<std::string> extraHeaders;
							std::string body = cgiOutput;

							size_t headerEnd = cgiOutput.find("\r\n\r\n");
							size_t headerBodySepLen = 4;
							if (headerEnd == std::string::npos)
							{
								headerEnd = cgiOutput.find("\n\n");
								headerBodySepLen = 2;
							}
							if (headerEnd != std::string::npos)
							{
								std::string headerSection = cgiOutput.substr(0, headerEnd);
								body = cgiOutput.substr(headerEnd + headerBodySepLen);
								std::istringstream headerStream(headerSection);
								std::string line;
								while (std::getline(headerStream, line))
								{
									if (!line.empty() && line[line.size() - 1] == '\r')
										line.erase(line.size() - 1);
									if (line.empty())
										continue;

									size_t colon = line.find(':');
									if (colon == std::string::npos)
										continue;

									std::string originalKey = trimAsciiEngine(line.substr(0, colon));
									std::string loweredKey = toLowerAsciiEngine(originalKey);
									std::string value = trimAsciiEngine(line.substr(colon + 1));

									if (value.empty())
										continue;
									if (loweredKey == "status")
										status = value;
									else if (loweredKey == "content-type")
										contentType = value;
									else if (loweredKey != "content-length" && loweredKey != "connection")
										extraHeaders.push_back(originalKey + ": " + value);
								}
							}

							conn->getWriteBuffer().append(
								buildResponse(status, body, contentType, conn->shouldClose(), extraHeaders)
							);
						}
						conn->clearCGI();
						conn->setState(Connection::WRITING);
						break;
					}
					else
					{
						if (errno == EAGAIN || errno == EWOULDBLOCK)
						{
							struct timeval now;
							gettimeofday(&now, NULL);
							time_t elapsed_sec = now.tv_sec - start_time.tv_sec;
							suseconds_t elapsed_usec = now.tv_usec - start_time.tv_usec;
							if (elapsed_usec < 0)
							{
								elapsed_sec -= 1;
								elapsed_usec += 1000000;
							}
							if (elapsed_sec >= 3)
							{
								pid_t pid = conn->getCGIPid();
								if (pid > 0)
									kill(pid, SIGKILL);
								if (pid > 0)
									waitpid(pid, NULL, 0);
								conn->getWriteBuffer() = buildErrorResponse(504, "Gateway Timeout", conn->shouldClose(), NULL);
								conn->setShouldClose(true);
								conn->clearCGI();
								conn->setState(Connection::WRITING);
								break;
							}
							break;
						}
						perror("read CGI");
						conn->setShouldClose(true);
						conn->getWriteBuffer() = buildErrorResponse(500, "Internal Server Error", conn->shouldClose(), NULL);
						conn->clearCGI();
						conn->setState(Connection::WRITING);
						break;
					}
				}
			}
			++it;
			continue;
		}

		if (FD_ISSET(clientFd, &readSet))
		{
			char buffer[8192];
			ssize_t bytes = recv(clientFd, buffer, sizeof(buffer), 0);
			if (bytes > 0)
			{
				handleClientRequest(it->second, buffer, bytes);
				++it;
				continue;
			}
			else if (bytes == 0)
			{
				delete it->second;
				_clientListenEndpoints.erase(clientFd);
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
				delete it->second;
				_clientListenEndpoints.erase(clientFd);
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
		Connection* conn = it->second;

		if (conn->getState() == Connection::CGI_RUNNING)
		{
			Connection::CGIContext* cgi = conn->getCGI();
			int cgi_in = conn->getCGIStdinFd();
			if (cgi != NULL && cgi_in != -1 && !conn->isCGIStdinClosed() && FD_ISSET(cgi_in, &writeSet))
			{
				if (cgi->stdin_offset < cgi->stdin_buffer.size())
				{
					const char* data = cgi->stdin_buffer.data() + cgi->stdin_offset;
					size_t remaining = cgi->stdin_buffer.size() - cgi->stdin_offset;
					ssize_t written = write(cgi_in, data, remaining);
					if (written > 0)
					{
						cgi->stdin_offset += static_cast<size_t>(written);
						conn->updateActivity();
					}
					else if (written < 0)
					{
						if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EPIPE)
						{
							perror("write CGI stdin");
							close(cgi_in);
							conn->setCGIStdinFd(-1);
							conn->setCGIStdinClosed(true);
						}
						else if (errno == EPIPE)
						{
							close(cgi_in);
							conn->setCGIStdinFd(-1);
							conn->setCGIStdinClosed(true);
						}
					}
				}
				if (cgi->stdin_offset >= cgi->stdin_buffer.size() && !conn->isCGIStdinClosed())
				{
					close(cgi_in);
					conn->setCGIStdinFd(-1);
					conn->setCGIStdinClosed(true);
				}
			}
		}
		
		if (!FD_ISSET(clientFd, &writeSet))
		{
			++it;
			continue;
		}

		std::string& writebuffer = conn->getWriteBuffer();
		if (writebuffer.empty())
		{
			if (conn->shouldClose())
			{
				delete conn;
				_clientListenEndpoints.erase(clientFd);
				_clientConnections.erase(it++);
				continue;
			}

			conn->setRequestState(Connection::READING_HEADERS);
			conn->setState(Connection::READING);
			++it;
			continue;
		}

		
		ssize_t sentByte = send(clientFd, writebuffer.data(), writebuffer.size(), 0);
		if (sentByte > 0)
		{
			writebuffer.erase(0, sentByte);
			conn->updateActivity();

			if (writebuffer.empty())
			{
				if (conn->shouldClose())
				{
					delete conn;
					_clientListenEndpoints.erase(clientFd);
					_clientConnections.erase(it++);
					continue;
				}
				conn->setRequestState(Connection::READING_HEADERS);
				conn->setState(Connection::READING);
			}
			++it;
			continue;
		}
		if (sentByte == 0)
		{
			delete conn;
			_clientListenEndpoints.erase(clientFd);
			_clientConnections.erase(it++);
			continue;
		}
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			++it;
			continue;
		}
		perror("send");
		delete conn;
		_clientListenEndpoints.erase(clientFd);
		_clientConnections.erase(it++);
	}
}

void Engine::checkTimeouts()
{
	const time_t headerTimeoutSec = 5;
	const time_t idleTimeoutSec = 30;
	const time_t writeTimeoutSec = 60;
	const time_t cgiTimeoutSec = 10;

	time_t now = std::time(NULL);
	for (std::map<int, Connection*>::iterator it = _clientConnections.begin();
			it != _clientConnections.end();)
	{
		int fd = it->first;
		Connection* conn = it->second;
		time_t elapsed = now - conn->getLastActivity();
		if (conn->getState() == Connection::WRITING && elapsed > writeTimeoutSec)
		{
			delete conn;
			_clientListenEndpoints.erase(fd);
			_clientConnections.erase(it++);
			continue;
		}

		if (conn->getState() == Connection::WRITING)
		{
			++it;
			continue;
		}

		std::map<int, std::pair<std::string, int> >::const_iterator epIt = _clientListenEndpoints.find(fd);
		const ServerConfig* serverConfig = NULL;
		if (epIt != _clientListenEndpoints.end())
			serverConfig = findServerConfig(epIt->second.first, epIt->second.second);

		if (conn->getState() == Connection::CGI_RUNNING)
		{
			Connection::CGIContext* cgi = conn->getCGI();
			if (cgi != NULL && cgi->start_time > 0 && (now - cgi->start_time) > cgiTimeoutSec)
			{
				pid_t pid = conn->getCGIPid();
				if (pid > 0)
				{
					kill(pid, SIGKILL);
					waitpid(pid, NULL, WNOHANG);
				}
				conn->clearCGI();
				conn->setShouldClose(true);
				conn->getWriteBuffer() = buildErrorResponse(504, "Gateway Timeout", true, serverConfig);
				conn->setState(Connection::WRITING);
			}
			++it;
			continue;
		}

		bool timedOut = false;
		if (conn->getRequestState() == Connection::READING_HEADERS && elapsed > headerTimeoutSec)
			timedOut = true;
		else if (conn->getState() == Connection::READING && elapsed > idleTimeoutSec)
			timedOut = true;

		if (timedOut)
		{
			conn->setShouldClose(true);
			conn->getReadBuffer().clear();
			conn->getWriteBuffer() = buildErrorResponse(408, "Request Timeout", true, serverConfig);
			conn->setRequestState(Connection::COMPLETE);
			conn->setState(Connection::WRITING);
			++it;
			continue;
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

	// sigint is ctrl + c, sigterm is system asked program to terminate
	std::signal(SIGINT, handleEngineStopSignal);
	std::signal(SIGTERM, handleEngineStopSignal);

	while (!g_engineStopRequested)
	{
		fd_set readSet;
		FD_ZERO(&readSet);
		fd_set writeSet;
		FD_ZERO(&writeSet);

		// range of fd to check for select
		int maxFd = 0;

		registerListenSocketsForSelect(readSet, maxFd);
		registerClientSocketForSelect(readSet, writeSet, maxFd);

		//wake up every 1 second (sleep up to 1 second) for checkTimeouts
		//time out is max sleep time
		struct timeval timeout;
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
		int readyFdCount = select(maxFd + 1, &readSet, &writeSet, NULL, &timeout);
		// if select was interrupted by a signal, eg: ctrl + c, sigterm...
		if (readyFdCount < 0)
		{
			if (errno == EINTR)
			{
				if (g_engineStopRequested) //only sigint sigterm stop it
					break;
				continue; //eg: sigchld should not stop this loop
			}
			perror("select");
			break;
		}
		checkTimeouts();
		acceptPendingClientConnections(readSet);
		processIncomingData(readSet);
		processOutgoingData(writeSet);

		// Reap any finished CGI child processes without blocking.
		while (waitpid(-1, NULL, WNOHANG) > 0);
	}
	std::cout << "Server stopping..." << std::endl;
}
