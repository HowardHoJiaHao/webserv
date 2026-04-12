/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   engine.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ho <hwai-keo@student.42kl.edu.my>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/19 20:53:37 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/04/13 03:03:41 by ho               ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Webserv.hpp"

#include <cstdlib>

static volatile sig_atomic_t g_engineStopRequested = 0;

// is this fd is valid to use select() / fd_set, if they are in this range
static bool isFdSelectable(int fd)
{
	return fd >= 0 && fd < FD_SETSIZE;
}

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
	std::srand(static_cast<unsigned int>(std::time(NULL)));
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

// example of serverConfigs
// refer to webserv.conf
// server{} <= first server
// server{} <= second server
//
// create listening socket for each server
void Engine::setupListeningSockets()
{
	const std::vector<ServerConfig>& serverConfigs = this->_config.getServers();
	for (std::vector<ServerConfig>::const_iterator it = serverConfigs.begin(); it != serverConfigs.end(); ++it)
	{
		// create one pair of host - port key value pair
		std::pair<std::string, int> key(it->getHost(), it->getPort());
		// listeningSocket store multiple entry of host port key value pair map to fd
		if (_listenSockets.find(key) == _listenSockets.end())// if this key cant find in existing listening socket
		{
			//create new one
			int fd = createListeningSocket(it->getHost(), it->getPort());
			// if 1024 listning socket, then throw exception and program ends
			if (!isFdSelectable(fd))
			{
				if (fd >= 0)
					close(fd);
				throw std::runtime_error("Listening socket fd exceeds FD_SETSIZE for select()");
			}
			// store it as new entry
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
// register all the fd from the listening socket to readSet, FOR THIS FUNCTION
void Engine::registerListenSocketsForSelect(fd_set& readSet, int& maxFd)
{
	for (std::map<std::pair<std::string, int>, int>::const_iterator it = _listenSockets.begin();
		it != _listenSockets.end(); ++it)
	{
		//std::cout << "tracing listen fd=" << it->second << std::endl;
		int fd = it->second;
		// if the fd is not safe, then skip it, do not register it into readSet
		if (!isFdSelectable(fd))
			continue;
		FD_SET(fd, &readSet);
		if (fd > maxFd)
			maxFd = fd;
	}
}

// for the first call, usually nothing in clientConnection and cgi fd until later created
//
// pointer cannot be store, reassigned or managed inside a container, but reference can
// pointer enable persistency and changing state and stored in map
void Engine::registerClientSocketForSelect(fd_set& readSet, fd_set& writeSet, int& maxFd)
{
	for (std::map<int, Connection*>::iterator it = _clientConnections.begin();
		it != _clientConnections.end();)
	{
		//std::cout << "tracking client fd=" << it->first << std::endl;
		Connection* currentConn = it->second;
		int fd = it->first;
		bool dropConnection = false;

		// if existing client socket want to suport cgi, it will extend new stdin and stdout fd
		// has risk of exceeding FD_SETSIZE, these cgi fd will be watch by select()
		if (!isFdSelectable(fd))
			dropConnection = true;

		// safeguard cgi related fd against select() limit
		if (currentConn->getState() == Connection::CGI_RUNNING)
		{
			int cgiOutputFd = currentConn->getCGIOutputFd();
			Connection::CGIContext* cgi = currentConn->getCGI();
			// check cgi stdout fd valid for select limit
			if (cgiOutputFd != -1 && !isFdSelectable(cgiOutputFd))
				dropConnection = true;
			// check cgi stdin fd valid for select limit
			int cgiInputFd = currentConn->getCGIInputFd();
			if (cgi != NULL && cgiInputFd != -1 && !isFdSelectable(cgiInputFd))
				dropConnection = true;
		}

		// drop current connection if fd is outside select() range
		if (dropConnection)
		{
			std::cerr << "Dropping connection with fd outside select() range (FD_SETSIZE="
					  << FD_SETSIZE << ")" << std::endl;
			destroyClientConnection(it);
			continue;
		}
		// register fd into readSet and writeSet, so select() can monitor it
		if (currentConn->getState() == Connection::READING)
			FD_SET(fd, &readSet);
		if (currentConn->getState() == Connection::WRITING)
			FD_SET(fd, &writeSet);
		// watch cgi stdout, then read from it
		// watch cgi stdin, then write to it
		if (currentConn->getState() == Connection::CGI_RUNNING)
		{
			Connection::CGIContext* cgi = currentConn->getCGI();
			int cgiOutputFd = currentConn->getCGIOutputFd();
			if (cgiOutputFd != -1)
			{
				FD_SET(cgiOutputFd, &readSet);
				if (cgiOutputFd > maxFd)
					maxFd = cgiOutputFd;
			}
			int cgiInputFd = currentConn->getCGIInputFd();
			if (cgi != NULL // cgi exist
				&& cgiInputFd != -1 // stdin pipe is valid
				&& !currentConn->isCGIInputClosed() // stdin pipe is not closed
				&& cgi->stdin_offset < cgi->stdin_buffer.size()) // there are still data left to read
			{
				FD_SET(cgiInputFd, &writeSet);
				if (cgiInputFd > maxFd)
					maxFd = cgiInputFd;
			}
		}
		// update and track highest fd
		if (fd > maxFd)
			maxFd = fd;
		++it;
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
			if (clientFd < 0)
			{
				// if exceed the os fd limit
				if (errno == EMFILE || errno == ENFILE)
					std::cerr << "accept failed: file descriptor table full" << std::endl;
				continue;
			}
			// if exceed the select() fd limit, close it and skip handling
			if (!isFdSelectable(clientFd))
			{
				std::cerr << "Rejecting client fd=" << clientFd
						  << " because it exceeds FD_SETSIZE=" << FD_SETSIZE << std::endl;
				close(clientFd);
				continue;
			}

			// normal path, create connection object
			if (fcntl(clientFd, F_SETFL, O_NONBLOCK) == -1)
			{
				close(clientFd);
				continue;
			}
			_clientConnections[clientFd] = new Connection(clientFd);
			// it->first refers to the key of the map, which is a pair of host and port, of the serverConfig
			_clientConnections[clientFd]->setServerConfig(findServerConfig(it->first.first, it->first.second));
		}
	}
}

// util
// find the serverConfig based on host and port, return as pointer
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
		// rebuild fd set in every loop
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

// example of serverConfig
// ServerConfig {
// host = "127.0.0.1";
// port = 8080;

// root = "./www1";
// index = "index.html";
// client_max_body_size = 1000000;

// error_pages = {
// 	404 -> "/404.html",
// 	500 -> "/500.html"
// };

// locations = {
// 	"/" -> {
// 		methods = ["GET", "POST"];
// 		autoindex = false;
// 	},
// 	"/haha" -> {
// 		methods = ["GET", "POST"];
// 		index = "upload.html";
// 		autoindex = true;
// 		root = "./www1";
// 	},
// 	"/cgi-bin" -> {
// 		methods = ["GET", "POST"];
// 		cgi_enabled = true;
// 		cgi_ext = [".py", ".pl"];
// 	},
// 	"/Upload" -> {
// 		methods = ["POST"];
// 		upload_enable = true;
// 		upload_path = "./uploads";
// 	}
// };
// }