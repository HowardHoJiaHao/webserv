/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   engineIncomingData.cpp                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hwai-keo <hwai-keo@student.42kl.edu.my>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/03 17:23:36 by hwai-keo          #+#    #+#             */
/*   Updated: 2026/04/15 15:44:57 by hwai-keo         ###   ########.fr       */
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
	const std::string pendingSetCookieHeader = currentConn->getPendingSetCookieHeader();

	if (cgiOutput.find("HTTP/1.") == 0)
	{
		if (!pendingSetCookieHeader.empty())
			currentConn->getWriteBuffer().append(appendHeaderToResponse(cgiOutput, pendingSetCookieHeader));
		else
			currentConn->getWriteBuffer().append(cgiOutput);
		currentConn->clearPendingSetCookieHeader();
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
	if (!pendingSetCookieHeader.empty())
		extraHeaders.push_back(pendingSetCookieHeader);

	currentConn->getWriteBuffer().append(
		buildResponse(status, body, contentType, currentConn->shouldClose(), extraHeaders)
	);
	currentConn->clearPendingSetCookieHeader();
}

void Engine::handleCGIReadError(Connection* currentConn)
{
	perror("read CGI");
	currentConn->setShouldClose(true);
	std::string response = buildErrorResponse(500, currentConn->shouldClose(), currentConn->getServerConfig());
	if (!currentConn->getPendingSetCookieHeader().empty())
	{
		response = appendHeaderToResponse(response, currentConn->getPendingSetCookieHeader());
		currentConn->clearPendingSetCookieHeader();
	}
	currentConn->setWriteBuffer(response);
	currentConn->clearCGI();
	currentConn->setState(Connection::WRITING);
}

bool Engine::processCGIOutput(Connection* currentConn, fd_set& readSet)
{
	int cgiOutputFd = currentConn->getCGIOutputFd();
	if (cgiOutputFd == -1 || !FD_ISSET(cgiOutputFd, &readSet))
		return false;

	char buffer[1024];
	Connection::CGIContext* cgi = currentConn->getCGI();
	ssize_t bytes = read(cgiOutputFd, buffer, sizeof(buffer));
	if (bytes > 0)
	{
		if (cgi != NULL)
			cgi->stdout_buffer.append(buffer, bytes);
		currentConn->updateLastActivity();
		return false;
	}
	if (bytes == 0)
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
	handleCGIReadError(currentConn);
	return true;
}

// input: http request from client, get, post, delete
// GET / HTTP/1.1
// Host: localhost:8080
// User-Agent: Mozilla/5.0
// Accept: text/html

// after keep alive => no connection? => timeout and connection drops, it has nothing to do with second if conditions
void Engine::handleClientSocketRead(std::map<int, Connection*>::iterator& it, int clientFd, fd_set& readSet)
{
	if (FD_ISSET(clientFd, &readSet))
	{
		char buffer[8192];
		// received whatever is currently available
		ssize_t bytes = recv(clientFd, buffer, sizeof(buffer), 0);
		if (bytes > 0)
		{
			// start the timer on the first byte of the request
			if (!it->second->hasStartedRequest())
			{
				it->second->setRequestStartTime(std::time(NULL));
				it->second->setHasStartedRequest(true);
			}
			handleClientRequest(it->second, buffer, bytes);
			++it;
		}
		// client has closed the connection, not request completion, only client side problem
		else if (bytes == 0)
		{
			destroyClientConnection(it);
		}
		// other error
		else
		{
			perror("recv");
			destroyClientConnection(it);
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
			// cgi not ready yet, try again next loop iteration
			else
			{
				++it;
				continue;
			}
		}
		handleClientSocketRead(it, clientFd, readSet);
	}
}
