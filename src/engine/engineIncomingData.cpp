/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   engineIncomingData.cpp                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: Ho Wai Keong <hwai_keo@student.42kl.edu    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/03 17:23:36 by hwai-keo          #+#    #+#             */
/*   Updated: 2026/04/04 23:33:42 by Ho Wai Keon      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Webserv.hpp"

void Engine::parseCGIHeaders(const std::string& headerSection, std::string& status, std::string& contentType, std::vector<std::string>& extraHeaders)
{
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

void Engine::buildResponseFromCGIOutput(Connection* currentConn, const std::string& cgiOutput)
{
	if (cgiOutput.find("HTTP/1.") == 0)
	{
		currentConn->getWriteBuffer().append(cgiOutput);
		return;
	}

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
		parseCGIHeaders(headerSection, status, contentType, extraHeaders);
	}

	currentConn->getWriteBuffer().append(
		buildResponse(status, body, contentType, currentConn->shouldClose(), extraHeaders)
	);
}

void Engine::handleCGIReadError(Connection* currentConn)
{
	perror("read CGI");
	currentConn->setShouldClose(true);
	currentConn->getWriteBuffer() = buildErrorResponse(500, "Internal Server Error", currentConn->shouldClose(), NULL);
	currentConn->clearCGI();
	currentConn->setState(Connection::WRITING);
}

bool Engine::handleCGIWouldBlock(Connection* currentConn, struct timeval& startTime)
{
	struct timeval now;
	gettimeofday(&now, NULL);
	time_t elapsed_sec = now.tv_sec - startTime.tv_sec;
	suseconds_t elapsed_usec = now.tv_usec - startTime.tv_usec;
	if (elapsed_usec < 0)
	{
		elapsed_sec -= 1;
		elapsed_usec += 1000000;
	}
	if (elapsed_sec < 3)
		return false;

	pid_t pid = currentConn->getCGIPid();
	if (pid > 0)
		kill(pid, SIGKILL);
	if (pid > 0)
		waitpid(pid, NULL, 0);
	currentConn->getWriteBuffer() = buildErrorResponse(504, "Gateway Timeout", currentConn->shouldClose(), NULL);
	currentConn->setShouldClose(true);
	currentConn->clearCGI();
	currentConn->setState(Connection::WRITING);
	return true;
}

bool Engine::processCGIOutput(Connection* currentConn, fd_set& readSet)
{
	int cgi_fd = currentConn->getCGIStdoutFd();
	if (cgi_fd == -1 || !FD_ISSET(cgi_fd, &readSet))
		return false;

	char buffer[1024];
	Connection::CGIContext* cgi = currentConn->getCGI();
	struct timeval start_time;
	gettimeofday(&start_time, NULL);
	while (true)
	{
		ssize_t bytes = read(cgi_fd, buffer, sizeof(buffer));
		if (bytes > 0)
		{
			if (cgi != NULL)
				cgi->stdout_buffer.append(buffer, bytes);
			currentConn->updateActivity();
		}
		else if (bytes == 0)
		{
			pid_t pid = currentConn->getCGIPid();
			if (pid > 0)
				waitpid(pid, NULL, WNOHANG);
			std::string cgiOutput;
			if (cgi != NULL)
				cgiOutput = cgi->stdout_buffer;
			buildResponseFromCGIOutput(currentConn, cgiOutput);
			currentConn->clearCGI();
			currentConn->setState(Connection::WRITING);
			return true;
		}
		else
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				if (handleCGIWouldBlock(currentConn, start_time))
					return true;
				return false;
			}
			handleCGIReadError(currentConn);
			return true;
		}
	}
}

// input: http request from client, get, post, delete
// GET / HTTP/1.1
// Host: localhost:8080
// User-Agent: Mozilla/5.0
// Accept: text/html


void Engine::handleClientSocketRead(std::map<int, Connection*>::iterator& it, int clientFd, fd_set& readSet)
{
	if (FD_ISSET(clientFd, &readSet))
	{
		char buffer[8192];
		// received whatever is currently available
		ssize_t bytes = recv(clientFd, buffer, sizeof(buffer), 0);
		if (bytes > 0)
		{
			handleClientRequest(it->second, buffer, bytes);
			++it;
		}
		else if (bytes == 0)
		{
			delete it->second;
			_clientListenEndpoints.erase(clientFd);
			_clientConnections.erase(it++);
		}
		else
		{
			// recv return -1 and set errno
			// nothing to read at this moment, but the connection is still open
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				++it;
			}
			else
			{
				perror("recv");
				delete it->second;
				_clientListenEndpoints.erase(clientFd);
				_clientConnections.erase(it++);
			}
		}
	}
	// this socket is not ready for reading, skip it
	else
		++it;
}

void Engine::processIncomingData(fd_set& readSet)
{
	for (std::map<int, Connection*>::iterator it = _clientConnections.begin();
			it != _clientConnections.end();)
	{
		int clientFd = it->first;
		Connection* currentConn = it->second;
		// conn is waiting for cgi process output
		if (currentConn->getState() == Connection::CGI_RUNNING)
		{
			// cgi processing is done
			// webserve(parent process) -> fork() -> cgi process(child) -> stdout (pipe) -> webserv reads
			if (processCGIOutput(currentConn, readSet))
			{
				++it;
				continue;
			}
			// cgi not ready yet(EAGAIN), try again next loop iteration
			else
			{
				++it;
				continue;
			}
		}
		handleClientSocketRead(it, clientFd, readSet);
	}
}
