/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   engine.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hwai-keo <hwai-keo@student.42kl.edu.my>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/19 20:53:37 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/03/29 15:53:39 by hwai-keo         ###   ########.fr       */
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
#include "extractRequest.hpp"
#include <ctime>
#include "Webserv.hpp"


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

const ServerConfig* Engine::findServerConfigForConnection(int clientFd, const HttpRequest& request) const
{
	std::map<int, std::pair<std::string, int> >::const_iterator epIt = _clientListenEndpoints.find(clientFd);
	if (epIt == _clientListenEndpoints.end())
		return findServerConfig("", 0);

	std::string host = epIt->second.first;
	int port = epIt->second.second;

	const std::string* hostHeader = request.getHeader("host");
	if (hostHeader != NULL && !hostHeader->empty())
	{
		std::string hostValue = *hostHeader;
		size_t colon = hostValue.rfind(':');
		if (colon != std::string::npos && colon + 1 < hostValue.size())
		{
			host = hostValue.substr(0, colon);
			std::istringstream iss(hostValue.substr(colon + 1));
			int parsedPort = port;
			if (!(iss >> parsedPort).fail())
				port = parsedPort;
		}
		else
		{
			host = hostValue;
		}
	}

	return findServerConfig(host, port);
}

const LocationConfig* Engine::findBestLocation(const ServerConfig& serverConfig, const std::string& path) const
{
	const std::vector<LocationConfig>& locations = serverConfig.getLocations();
	const LocationConfig* best = NULL;
	size_t bestLen = 0;

	for (size_t i = 0; i < locations.size(); ++i)
	{
		const std::string& locPath = locations[i].getPath();
		if (path.find(locPath) == 0 && locPath.size() >= bestLen)
		{
			best = &locations[i];
			bestLen = locPath.size();
		}
	}
	return best;
}

bool Engine::isMethodAllowed(const std::string& method, const LocationConfig* location) const
{
	if (location == NULL)
		return true;

	const std::vector<std::string>& allowed = location->getAllowedMethods();
	if (allowed.empty())
		return true;

	for (size_t i = 0; i < allowed.size(); ++i)
	{
		if (allowed[i] == method)
			return true;
	}
	return false;
}

// request line : 1) method, 2) path, 3) version, eg: GET / HTTP/1.1 \r\n
// header : 1) localhost, 2) content-length
// empty line
// body (optional)

void Engine::handleClientRequest(Connection* conn, const char* buffer, ssize_t bytes)
{
	conn->appendToReadBuffer(buffer, bytes);
	conn->updateActivity();

	const size_t maxHeaderSize = 8192;

	std::string& readBuffer = conn->getReadBuffer();
	size_t headerEnd = readBuffer.find("\r\n\r\n");
	if (headerEnd == std::string::npos)
	{
		if(readBuffer.size() > maxHeaderSize)
		{
			conn->setShouldClose(true);
			conn->getReadBuffer().clear();
			conn->getWriteBuffer() = buildResponse
			(
				"413 Payload Too Large",
				"Header Too Large",
				"text/plain",
				conn->shouldClose(),
				std::vector<std::string>()
			);
			conn->setState(Connection::WRITING);
			return;
		}
	}


	if (conn->getReadBuffer().size() > MAX_REQUEST_SIZE)
	{
		conn->setShouldClose(true);
		conn->getReadBuffer().clear();
		conn->getWriteBuffer() = buildResponse
		(
			"413 Payload Too Large",
			"Payload Too Large",
			"text/plain",
			conn->shouldClose(),
			std::vector<std::string>()
		);
		conn->setState(Connection::WRITING);
		return;
	}

	std::string rawRequest;
	bool producedResponse = false;
	if (headerEnd == std::string::npos)
	{
		conn->setRequestState(Connection::READING_HEADERS);
	}
	else
	{
		std::string testBuffer = readBuffer;
		std::string dummy;
		bool malformed = false;

		if (extractRequest(testBuffer, dummy, &malformed))
		{
			conn->setRequestState(Connection::COMPLETE);
		}
		else if (malformed)
		{
			conn->setShouldClose(true);
			conn->getWriteBuffer() = build400Response(conn->shouldClose(), NULL);
			conn->setState(Connection::WRITING);
			return;
		}
		else
		{
			conn->setRequestState(Connection::READING_BODY);
		}
	}

	while (true)
	{
		bool malformed = false;
		if (!extractRequest(readBuffer, rawRequest, &malformed))
		{
			if (malformed)
			{
				conn->setShouldClose(true);
				conn->getWriteBuffer() = build400Response(conn->shouldClose(), NULL);
				conn->setState(Connection::WRITING);
				return;
			}
			break;
		}

		HttpRequest request;
		std::vector<std::string> extraHeaders;
		const ServerConfig* defaultServer = findServerConfig(
			_clientListenEndpoints[conn->getFd()].first,
			_clientListenEndpoints[conn->getFd()].second
		);
		size_t maxBodySize = MAX_REQUEST_SIZE;
		if (defaultServer != NULL)
			maxBodySize = defaultServer->getMaxBodySize();
		try
		{
			request.parse(rawRequest, maxBodySize);

			std::string sessionId = request.getCookie("sessionId");
			

			if (sessionId.empty() || _sessions.find(sessionId) == _sessions.end())
			{
				std::cout << "New User\n";
				std::stringstream ss;
				ss << std::rand() << std::time(NULL);
				sessionId = ss.str();

				_sessions[sessionId] = 1;
				extraHeaders.push_back("Set-Cookie: sessionId=" + sessionId + "; Path=/");
			}
			else
			{
				_sessions[sessionId]++;
				std::cout << "Returning user: " << sessionId << " (visit " << _sessions[sessionId] << ")\n";
			}
		}
		catch (const std::exception& e)
		{
			conn->setShouldClose(true);
			if (std::string(e.what()) == "Body too large")
				conn->getWriteBuffer() = buildErrorResponse(413, "Payload Too Large", conn->shouldClose(), defaultServer);
			else
				conn->getWriteBuffer() = build400Response(conn->shouldClose(), defaultServer);
			conn->setState(Connection::WRITING);
			return;
		}

		const ServerConfig* serverConfig = findServerConfigForConnection(conn->getFd(), request);
		if (serverConfig != NULL && request.hasContentLength() && request.getContentLength() > serverConfig->getMaxBodySize())
		{
			conn->setShouldClose(true);
			conn->getWriteBuffer() = buildErrorResponse(413, "Payload Too Large", conn->shouldClose(), serverConfig);
			conn->setState(Connection::WRITING);
			return;
		}

		bool shouldClose = request.shouldCloseConnection();
		if (shouldClose)
			conn->setShouldClose(true);

		const ServerConfig* effectiveServer = defaultServer;
		if (serverConfig != NULL)
			effectiveServer = serverConfig;
		if (effectiveServer == NULL)
		{
			conn->setShouldClose(true);
			conn->getWriteBuffer() = buildErrorResponse(500, "Internal Server Error", conn->shouldClose(), NULL);
			conn->setState(Connection::WRITING);
			return;
		}
		std::string response = routeRequest(request, shouldClose, *effectiveServer);
		if (!extraHeaders.empty())
		{
			size_t pos = response.find("\r\n\r\n");
			if (pos != std::string::npos)
			{
				std::string headerPart = response.substr(0, pos);
				std::string bodyPart = response.substr(pos);

				for (size_t i = 0; i < extraHeaders.size(); i++)
					headerPart += "\r\n" + extraHeaders[i];

				response = headerPart + bodyPart;
			}
		}
		conn->getWriteBuffer() += response;
		producedResponse = true;
	}

	if (producedResponse)
		conn->setState(Connection::WRITING);
	else
		conn->setState(Connection::READING);
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
	time_t now = std::time(NULL);
	for (std::map<int, Connection*>::iterator it = _clientConnections.begin();
			it != _clientConnections.end();)
	{
		Connection* conn = it->second;
		time_t elapsed = now - conn->getLastActivity();
		bool timedOut = false;

		if (conn->getRequestState() == Connection::READING_HEADERS && elapsed > 5)
			timedOut = true;
		else if (elapsed > 30)
			timedOut = true;

		if (timedOut)
		{
			int fd = it->first;
			delete conn;
			_clientListenEndpoints.erase(fd);
			_clientConnections.erase(it++);
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
	while (true)
	{
		fd_set readSet;
		FD_ZERO(&readSet);
		fd_set writeSet;
		FD_ZERO(&writeSet);

		int maxFd = 0;

		registerListenSocketsForSelect(readSet, maxFd);
		registerClientSocketForSelect(readSet, writeSet, maxFd);

		struct timeval timeout;
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
		int readyFdCount = select(maxFd + 1, &readSet, &writeSet, NULL, &timeout);
		if (readyFdCount < 0)
		{
			if (errno == EINTR)
				continue;
			perror("select");
			break;
		}
		checkTimeouts();
		acceptPendingClientConnections(readSet);
		processIncomingData(readSet);
		processOutgoingData(writeSet);
	}
}

std::string Engine::routeRequest(const HttpRequest& request, bool shouldClose, const ServerConfig& serverConfig)
{
	const LocationConfig* location = findBestLocation(serverConfig, request.getPath());
	if (!isMethodAllowed(request.getMethod(), location))
		return build405Response(shouldClose, &serverConfig);

	std::string root = serverConfig.getRoot();
	if (location != NULL && !location->getRoot().empty())
		root = location->getRoot();

	if (request.getMethod() == "GET")
	{
		if (request.getPath().find("..") != std::string::npos)
			return buildErrorResponse(403, "Forbidden", shouldClose, &serverConfig);
		std::string path = FileHandler::resolvePath(request.getPath(), root, serverConfig.getIndex());
		struct stat s;
		if (stat(path.c_str(), &s) == 0 && S_ISDIR(s.st_mode))
		{
			std::string indexPath = path + "/" + serverConfig.getIndex();
			if (FileHandler::fileExists(indexPath))
				path = indexPath;
			else if (location != NULL && location->isAutoindex())
			{
				std::string listing = FileHandler::generateDirectoryListing(request.getPath(), path);
				if (listing.empty())
					return buildErrorResponse(500, "Internal Server Error", shouldClose, &serverConfig);
				return buildResponse("200 OK", listing, "text/html", shouldClose, std::vector<std::string>());
			}
			else
				return buildErrorResponse(403, "Forbidden", shouldClose, &serverConfig);
		}

		if (!FileHandler::fileExists(path))
			return build404Response(shouldClose, &serverConfig);
		std::string content = FileHandler::readFile(path);
		std::string mime = FileHandler::getMimeType(path);
		return buildResponse("200 OK", content, mime, shouldClose, std::vector<std::string>());
	}
	if (request.getMethod() == "POST")
		return handlePost(request, shouldClose, serverConfig, location);
	if (request.getMethod() == "DELETE")
		return handleDelete(request, shouldClose, serverConfig);
	return build405Response(shouldClose, &serverConfig);
}

std::string Engine::handlePost(const HttpRequest& request, bool shouldClose, const ServerConfig& serverConfig, const LocationConfig* location)
{
	const std::string& body = request.getBody();
	std::string uploadPath = serverConfig.getRoot() + "/upload.txt";
	if (location != NULL && location->isUploadEnabled() && !location->getUploadPath().empty())
		uploadPath = location->getUploadPath() + "/upload.txt";

	int fd = open(uploadPath.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
	if (fd < 0)
		return buildErrorResponse(500, "Internal Server Error", shouldClose, &serverConfig);
	size_t total = 0;
	while (total < body.size())
	{
		ssize_t written = write(fd, body.data() + total, body.size() - total);
		if (written <= 0)
		{
			close(fd);
			return buildErrorResponse(500, "Internal Server Error", shouldClose, &serverConfig);
		}
		total += written;
	}
	close(fd);
	return buildResponse("201 Created", "Upload OK", "text/plain", shouldClose, std::vector<std::string>());
}

std::string Engine::handleDelete(const HttpRequest& request, bool shouldClose, const ServerConfig& serverConfig)
{
	if (request.getPath().find("..") != std::string::npos)
		return buildErrorResponse(403, "Forbidden", shouldClose, &serverConfig);

	const LocationConfig* location = findBestLocation(serverConfig, request.getPath());
	std::string root = serverConfig.getRoot();
	if (location != NULL && !location->getRoot().empty())
		root = location->getRoot();

	std::string path = FileHandler::resolvePath(request.getPath(), root, serverConfig.getIndex());
	if (!FileHandler::fileExists(path))
		return build404Response(shouldClose, &serverConfig);

	if (std::remove(path.c_str()) != 0)
		return buildErrorResponse(500, "Internal Server Error", shouldClose, &serverConfig);

	return buildResponse("204 No Content", "", "text/plain", shouldClose, std::vector<std::string>());
}

std::string Engine::buildResponse
(
	const std::string& status,
	const std::string& body,
	const std::string& contentType,
	bool shouldClose,
	const std::vector<std::string>& extraHeaders
)
{
	std::stringstream ss;
	ss << "HTTP/1.1 " << status << "\r\n";
	ss << "Content-Length: " << body.size() << "\r\n";
	ss << "Content-Type: " << contentType << "\r\n";

	for (size_t i = 0; i < extraHeaders.size(); i++)
	{
		ss << extraHeaders[i] << "\r\n";
	}
	if (shouldClose)
		ss << "Connection: close\r\n";
	else
		ss << "Connection: keep-alive\r\n";
	ss << "\r\n";
	ss << body;
	return ss.str();
}

std::string Engine::buildErrorResponse(int code, const std::string& defaultMsg, bool shouldClose, const ServerConfig* serverConfig)
{
	if (serverConfig != NULL)
	{
		const std::string* pagePath = serverConfig->getErrorPage(code);
		if (pagePath != NULL)
		{
			std::string fullPath = serverConfig->getRoot() + *pagePath;
			if (FileHandler::fileExists(fullPath))
			{
				std::string body = FileHandler::readFile(fullPath);
				std::ostringstream status;
				status << code << " " << defaultMsg;
				return buildResponse(status.str(), body, "text/html", shouldClose, std::vector<std::string>());
			}
		}
	}

	std::ostringstream status;
	status << code << " " << defaultMsg;
	return buildResponse(status.str(), defaultMsg, "text/plain", shouldClose, std::vector<std::string>());
}

std::string Engine::build405Response(bool shouldClose, const ServerConfig* serverConfig)
{
	return buildErrorResponse(405, "Method Not Allowed", shouldClose, serverConfig);
}

std::string Engine::buildIndexResponse()
{
	return buildResponse("200 OK", "Hello, world!", "text/plain", false, std::vector<std::string>());
}

std::string Engine::build404Response(bool shouldClose, const ServerConfig* serverConfig)
{
	return buildErrorResponse(404, "Not Found", shouldClose, serverConfig);
}

std::string Engine::build400Response(bool shouldClose, const ServerConfig* serverConfig)
{
	return buildErrorResponse(400, "Bad Request", shouldClose, serverConfig);
}
