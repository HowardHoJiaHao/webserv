/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   engine.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hwai-keo <hwai-keo@student.42kl.edu.my>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/19 20:53:37 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/03/31 14:16:35 by hwai-keo         ###   ########.fr       */
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
#include <cctype>
#include "httpHandling/httpRequest.hpp"
#include "FileHandler.hpp"
#include <sys/stat.h>
#include "extractRequest.hpp"
#include <ctime>
#include "Webserv.hpp"
#include <sys/wait.h>

static bool hasSuffix(const std::string& value, const std::string& suffix)
{
	if (value.size() < suffix.size())
		return false;
	return value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static bool isCGIRequestPath(const std::string& path)
{
	if (path.find("/cgi-bin/") == 0 || path == "/cgi-bin")
		return true;
	if (hasSuffix(path, ".py") || hasSuffix(path, ".pl"))
		return true;
	return false;
}

static std::string toLowerAsciiEngine(const std::string& input)
{
	std::string lowered = input;
	for (size_t i = 0; i < lowered.size(); ++i)
		lowered[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lowered[i])));
	return lowered;
}

static std::string trimAsciiEngine(const std::string& input)
{
	size_t start = 0;
	while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start])))
		++start;
	size_t end = input.size();
	while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1])))
		--end;
	return input.substr(start, end - start);
}

static bool isUploadFilenameCharEngine(char c)
{
	unsigned char uc = static_cast<unsigned char>(c);
	if (std::isalnum(uc))
		return true;
	return (c == '.' || c == '_' || c == '-');
}

static std::string sanitizeUploadFilenameEngine(const std::string& raw)
{
	std::string out;
	out.reserve(raw.size());
	for (size_t i = 0; i < raw.size(); ++i)
	{
		if (isUploadFilenameCharEngine(raw[i]))
			out.push_back(raw[i]);
		else if (raw[i] == '/' || raw[i] == '\\')
			continue;
		else
			out.push_back('_');
	}
	if (out == "." || out == "..")
		out.clear();
	if (out.size() > 128)
		out.erase(128);
	return out;
}

static std::string extractUploadFilenameEngine(const HttpRequest& request)
{
	const std::string* contentDisposition = request.getHeader("content-disposition");
	if (contentDisposition == NULL || contentDisposition->empty())
		return "";

	const std::string& headerValue = *contentDisposition;
	std::string lowered = toLowerAsciiEngine(headerValue);
	size_t keyPos = lowered.find("filename=");
	if (keyPos == std::string::npos)
		return "";

	size_t valueStart = keyPos + 9;
	if (valueStart >= headerValue.size())
		return "";

	size_t valueEnd = std::string::npos;
	if (headerValue[valueStart] == '"' || headerValue[valueStart] == '\'')
	{
		char quote = headerValue[valueStart];
		++valueStart;
		valueEnd = headerValue.find(quote, valueStart);
	}
	else
	{
		valueEnd = headerValue.find(';', valueStart);
	}

	if (valueEnd == std::string::npos)
		valueEnd = headerValue.size();
	if (valueEnd <= valueStart)
		return "";

	std::string extracted = trimAsciiEngine(headerValue.substr(valueStart, valueEnd - valueStart));
	return sanitizeUploadFilenameEngine(extracted);
}

static std::string defaultUploadFilenameEngine()
{
	std::ostringstream oss;
	oss << "upload_" << std::time(NULL) << "_" << std::rand() << ".bin";
	return oss.str();
}

static bool ensureDirectoryExistsEngine(const std::string& path)
{
	if (path.empty())
		return false;

	struct stat st;
	if (stat(path.c_str(), &st) == 0)
		return S_ISDIR(st.st_mode);

	if (mkdir(path.c_str(), 0755) == 0)
		return true;

	if (errno == EEXIST && stat(path.c_str(), &st) == 0)
		return S_ISDIR(st.st_mode);
	return false;
}



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
		if (path.find(locPath) != 0)
			continue;

		bool boundaryMatch = (locPath == "/"
			|| path.size() == locPath.size()
			|| path[locPath.size()] == '/');
		if (boundaryMatch && locPath.size() >= bestLen)
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
	const ServerConfig* defaultServerForLimit = findServerConfig(
		_clientListenEndpoints[conn->getFd()].first,
		_clientListenEndpoints[conn->getFd()].second
	);
	size_t maxBodySizeLimit = MAX_REQUEST_SIZE;
	if (defaultServerForLimit != NULL)
		maxBodySizeLimit = defaultServerForLimit->getMaxBodySize();
	size_t maxBufferedRequestSize = maxBodySizeLimit + maxHeaderSize;
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


	if (conn->getReadBuffer().size() > maxBufferedRequestSize)
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
		conn->setRequestState(Connection::READING_HEADERS);
	else
		conn->setRequestState(Connection::READING_BODY);

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
			if (readBuffer.find("\r\n\r\n") == std::string::npos)
				conn->setRequestState(Connection::READING_HEADERS);
			else
				conn->setRequestState(Connection::READING_BODY);
			break;
		}
		conn->setRequestState(Connection::COMPLETE);

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

		const LocationConfig* matchedLocation = findBestLocation(*effectiveServer, request.getPath());
		if (!isMethodAllowed(request.getMethod(), matchedLocation))
		{
			conn->getWriteBuffer() = build405Response(conn->shouldClose(), effectiveServer, matchedLocation);
			conn->setState(Connection::WRITING);
			return;
		}

		if (isCGIRequestPath(request.getPath()))
		{
			if (!launchCGI(conn, request, *effectiveServer, conn->shouldClose()))
				conn->setState(Connection::WRITING);
			return;
		}

		std::string sessionId = request.getCookie("sessionId");

		if (sessionId.empty() || _sessions.find(sessionId) == _sessions.end())
		{
			std::stringstream ss;
			ss << std::rand() << std::time(NULL);
			sessionId = ss.str();

			_sessions[sessionId] = 1;
			extraHeaders.push_back("Set-Cookie: sessionId=" + sessionId + "; Path=/");
		}
		else
		{
			_sessions[sessionId]++;
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

		Connection* conn = it->second;

		if (conn->getState() == Connection::CGI_RUNNING)
		{
			int cgi_fd = conn->getCGIStdoutFd();
			if (cgi_fd != -1 && FD_ISSET(cgi_fd, &readSet))
			{
				char buffer[1024];
				Connection::CGIContext* cgi = conn->getCGI();
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
							waitpid(pid, NULL, 0);

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
					waitpid(pid, NULL, 0);
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
	if (location != NULL && location->hasReturnDirective())
	{
		std::vector<std::string> headers;
		headers.push_back("Location: " + location->getReturnTarget());
		std::string status = "302 Found";
		if (location->getReturnStatus() == 301)
			status = "301 Moved Permanently";
		else if (location->getReturnStatus() == 303)
			status = "303 See Other";
		else if (location->getReturnStatus() == 307)
			status = "307 Temporary Redirect";
		else if (location->getReturnStatus() == 308)
			status = "308 Permanent Redirect";
		return buildResponse(status, "", "text/plain", shouldClose, headers);
	}
	if (!isMethodAllowed(request.getMethod(), location))
		return build405Response(shouldClose, &serverConfig, location);

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
	return build405Response(shouldClose, &serverConfig, location);
}

std::string Engine::handlePost(const HttpRequest& request, bool shouldClose, const ServerConfig& serverConfig, const LocationConfig* location)
{
	const std::string& body = request.getBody();
	std::string uploadDir = serverConfig.getRoot();
	if (location != NULL && location->isUploadEnabled() && !location->getUploadPath().empty())
		uploadDir = location->getUploadPath();

	if (!ensureDirectoryExistsEngine(uploadDir))
		return buildErrorResponse(500, "Internal Server Error", shouldClose, &serverConfig);

	std::string filename = extractUploadFilenameEngine(request);
	if (filename.empty())
		filename = defaultUploadFilenameEngine();

	int fd = -1;
	std::string uploadPath;
	for (int attempt = 0; attempt < 10; ++attempt)
	{
		uploadPath = uploadDir + "/" + filename;
		fd = open(uploadPath.c_str(), O_CREAT | O_WRONLY | O_EXCL, 0644);
		if (fd >= 0)
			break;
		if (errno != EEXIST)
			return buildErrorResponse(500, "Internal Server Error", shouldClose, &serverConfig);
		filename = defaultUploadFilenameEngine();
	}
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

std::string Engine::build405Response(bool shouldClose, const ServerConfig* serverConfig, const LocationConfig* location)
{
	std::string allowValue = "GET, POST, DELETE";
	if (location != NULL)
	{
		const std::vector<std::string>& allowed = location->getAllowedMethods();
		if (!allowed.empty())
		{
			allowValue.clear();
			for (size_t i = 0; i < allowed.size(); ++i)
			{
				if (i > 0)
					allowValue += ", ";
				allowValue += allowed[i];
			}
		}
	}

	std::vector<std::string> headers;
	headers.push_back("Allow: " + allowValue);

	if (serverConfig != NULL)
	{
		const std::string* pagePath = serverConfig->getErrorPage(405);
		if (pagePath != NULL)
		{
			std::string fullPath = serverConfig->getRoot() + *pagePath;
			if (FileHandler::fileExists(fullPath))
			{
				std::string body = FileHandler::readFile(fullPath);
				return buildResponse("405 Method Not Allowed", body, "text/html", shouldClose, headers);
			}
		}
	}

	return buildResponse("405 Method Not Allowed", "Method Not Allowed", "text/plain", shouldClose, headers);
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

bool	Engine::launchCGI(Connection* conn, const HttpRequest& request, const ServerConfig& serverConfig, bool shouldClose)
{
	if (request.getPath().find("..") != std::string::npos)
	{
		conn->setShouldClose(true);
		conn->getWriteBuffer() = buildErrorResponse(403, "Forbidden", true, &serverConfig);
		return false;
	}

	std::string root = serverConfig.getRoot();
	const LocationConfig* location = findBestLocation(serverConfig, request.getPath());
	if (location != NULL && !location->getRoot().empty())
		root = location->getRoot();

	std::string scriptPath = root + request.getPath();
	if (!FileHandler::fileExists(scriptPath))
	{
		conn->setShouldClose(shouldClose);
		conn->getWriteBuffer() = build404Response(conn->shouldClose(), &serverConfig);
		return false;
	}
	if (access(scriptPath.c_str(), X_OK) != 0)
	{
		conn->setShouldClose(true);
		conn->getWriteBuffer() = buildErrorResponse(403, "Forbidden", true, &serverConfig);
		return false;
	}

	int in_pipe[2];
	int out_pipe[2];

	if (pipe(in_pipe) < 0)
	{
		perror("pipe in_pipe failed");
		conn->setShouldClose(true);
		conn->getWriteBuffer() = buildErrorResponse(500, "Internal Server Error", true, &serverConfig);
		return false;
	}

	if (pipe(out_pipe) < 0)
	{
		perror("pipe out_pip failed");
		close(in_pipe[0]);
		close(in_pipe[1]);
		conn->setShouldClose(true);
		conn->getWriteBuffer() = buildErrorResponse(500, "Internal Server Error", true, &serverConfig);
		return false;
	}
	pid_t pid = fork();
	if (pid < 0)
	{
		perror("fork failed");
		close(in_pipe[0]);
		close(in_pipe[1]);
		close(out_pipe[0]);
		close(out_pipe[1]);
		conn->setShouldClose(true);
		conn->getWriteBuffer() = buildErrorResponse(500, "Internal Server Error", true, &serverConfig);
		return false;
	}
	if (pid == 0)
	{
		if (dup2(in_pipe[0], STDIN_FILENO) < 0)
		{
			perror("dup2 stdin failed");
			_exit(1);
		}
		if (dup2(out_pipe[1], STDOUT_FILENO) < 0)
		{
			perror("dup2 stdout failed");
			_exit(1);
		}
		close(in_pipe[0]);
		close(in_pipe[1]);
		close(out_pipe[0]);
		close(out_pipe[1]);

		std::vector<std::string> envStrings;
		envStrings.push_back("REQUEST_METHOD=" + request.getMethod());
		envStrings.push_back("QUERY_STRING=" + request.getQuery());
		envStrings.push_back("SCRIPT_NAME=" + request.getPath());
		envStrings.push_back("PATH_INFO=" + request.getPath());
		envStrings.push_back("SERVER_PROTOCOL=" + request.getVersion());
		envStrings.push_back("GATEWAY_INTERFACE=CGI/1.1");

		std::ostringstream contentLength;
		contentLength << request.getBody().size();
		envStrings.push_back("CONTENT_LENGTH=" + contentLength.str());

		const std::string* contentTypeHeader = request.getHeader("content-type");
		if (contentTypeHeader != NULL && !contentTypeHeader->empty())
			envStrings.push_back("CONTENT_TYPE=" + *contentTypeHeader);
		else if (request.getMethod() == "POST")
			envStrings.push_back("CONTENT_TYPE=application/octet-stream");

		const std::string* hostHeader = request.getHeader("host");
		if (hostHeader != NULL)
			envStrings.push_back("HTTP_HOST=" + *hostHeader);

		std::vector<char*> envp;
		for (size_t i = 0; i < envStrings.size(); ++i)
			envp.push_back(const_cast<char*>(envStrings[i].c_str()));
		envp.push_back(NULL);

		char* argv[] = { const_cast<char*>(scriptPath.c_str()), NULL};

		execve(scriptPath.c_str(), argv, &envp[0]);
		perror("execve failed");
		_exit(1);
	}

	close(in_pipe[0]);
	close(out_pipe[1]);

	Connection::CGIContext* cgi = new Connection::CGIContext();
	cgi->pid = pid;
	cgi->stdin_fd = in_pipe[1];
	cgi->stdout_fd = out_pipe[0];
	cgi->stdin_closed = false;
	cgi->stdin_buffer = request.getBody();
	cgi->stdin_offset = 0;
	cgi->stdout_buffer.clear();
	cgi->start_time = std::time(NULL);

	int stdinFlags = fcntl(cgi->stdin_fd, F_GETFL, 0);
	if (stdinFlags != -1)
		fcntl(cgi->stdin_fd, F_SETFL, stdinFlags | O_NONBLOCK);
	int stdoutFlags = fcntl(cgi->stdout_fd, F_GETFL, 0);
	if (stdoutFlags != -1)
		fcntl(cgi->stdout_fd, F_SETFL, stdoutFlags | O_NONBLOCK);

	if (cgi->stdin_buffer.empty())
	{
		close(cgi->stdin_fd);
		cgi->stdin_fd = -1;
		cgi->stdin_closed = true;
	}

	conn->setCGI(cgi);
	conn->setShouldClose(shouldClose);
	conn->setState(Connection::CGI_RUNNING);
	conn->updateActivity();
	return true;
}