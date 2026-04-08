/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   engine.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hwai-keo <hwai-keo@student.42kl.edu.my>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/19 20:53:37 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/04/08 17:29:26 by hwai-keo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Webserv.hpp"

#include <cstdlib>

static volatile sig_atomic_t g_engineStopRequested = 0;

static void handleEngineStopSignal(int)
{
	g_engineStopRequested = 1;
}

Engine::Engine(const ConfigFiles& config)
	: _config(config),
	  _listenSockets(),
	  _clientConnections(),
	  _sessions(),
	  _sessionCounter(0)
{
	std::srand(static_cast<unsigned int>(std::time(NULL)) ^ static_cast<unsigned int>(::getpid()));
}

//close fd, destructor
Engine::~Engine()
{
	for (std::map<int, Connection*>::iterator it = _clientConnections.begin();
		it != _clientConnections.end(); ++it)
	{
		Connection* currentConn = it->second;
		if (!currentConn)
			continue;
		const pid_t pid = currentConn->getCGIPid();
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

// create a new clientSocketConnection socket
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
				if (fcntl(clientFd, F_SETFL, O_NONBLOCK) == -1)
				{
					close(clientFd);
					continue;
				}
				_clientConnections[clientFd] = new Connection(clientFd);
				_clientConnections[clientFd]->setServerConfig(findServerConfig(it->first.first, it->first.second));
			}
		}
	}
}

// util
const ServerConfig* Engine::findServerConfig(const std::string& host, int port) const
{
	const std::vector<ServerConfig>& servers = _config.getServers();
	for (size_t i = 0; i < servers.size(); ++i)
	{
		if (servers[i].getHost() == host && servers[i].getPort() == port)
			return &servers[i];
	}
	return NULL;
}

void Engine::destroyClientConnection(std::map<int, Connection*>::iterator& it)
{
	delete it->second;
	_clientConnections.erase(it++);
}

void Engine::checkTimeouts()
{
	const time_t headerTimeoutSec = 5;
	const time_t bodyReadTimeout = 30;
	const time_t writeTimeoutSec = 60;
	const time_t cgiTimeoutSec = 10;

	time_t now = std::time(NULL);
	for (std::map<int, Connection*>::iterator it = _clientConnections.begin();
			it != _clientConnections.end();)
	{
		Connection* currentConn = it->second;
		time_t elapsed = now - currentConn->getLastActivity();
		if (currentConn->getState() == Connection::WRITING && elapsed > writeTimeoutSec)
		{
			destroyClientConnection(it);
			continue;
		}

		if (currentConn->getState() == Connection::WRITING)
		{
			++it;
			continue;
		}
		// for buildErrorResponse
		const ServerConfig* serverConfig = currentConn->getServerConfig();

		// if cgi script run too long, kill it, clean up, send 504 eror
		if (currentConn->getState() == Connection::CGI_RUNNING)
		{
			Connection::CGIContext* cgi = currentConn->getCGI();
			if (cgi != NULL && cgi->start_time > 0 && (now - cgi->start_time) > cgiTimeoutSec)
			{
				pid_t pid = currentConn->getCGIPid();
				if (pid > 0)
				{
					kill(pid, SIGKILL);
					waitpid(pid, NULL, WNOHANG);
				}
				currentConn->clearCGI();
				currentConn->setShouldClose(true);
				currentConn->setWriteBuffer(buildErrorResponse(504, true, serverConfig));
				currentConn->setState(Connection::WRITING);
			}
			++it;
			continue;
		}

		bool timedOut = false;
		if (currentConn->getRequestState() == Connection::READING_HEADERS && elapsed > headerTimeoutSec)
			timedOut = true;
		else if (currentConn->getState() == Connection::READING && elapsed > bodyReadTimeout)
			timedOut = true;

		if (timedOut)
		{
			currentConn->setShouldClose(true);
			currentConn->getReadBuffer().clear();
			currentConn->setWriteBuffer(buildErrorResponse(408, true, serverConfig));
			currentConn->setRequestState(Connection::COMPLETE);
			currentConn->setState(Connection::WRITING);
			++it;
			continue;
		}
		++it;
	}
}

// fd_set is a box of switches indexed by fd number
// ft_set is a bitmask(array of bits)
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
