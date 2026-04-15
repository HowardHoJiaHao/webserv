/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   engineOutgoingData.cpp                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ho <hwai-keo@student.42kl.edu.my>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/03 17:23:36 by hwai-keo          #+#    #+#             */
/*   Updated: 2026/04/14 00:30:23 by ho               ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Webserv.hpp"

void Engine::handleCGIStdinWrite(Connection* currentConn, fd_set& writeSet)
{
	Connection::CGIContext* cgi = currentConn->getCGI();
	int cgiInputFd = currentConn->getCGIInputFd();

	if (cgi == NULL || cgiInputFd == -1 || currentConn->isCGIInputClosed() || !FD_ISSET(cgiInputFd, &writeSet))
		return;

	if (cgi->stdin_offset < cgi->stdin_buffer.size())
	{
		const char* data = cgi->stdin_buffer.data() + cgi->stdin_offset;
		size_t remaining = cgi->stdin_buffer.size() - cgi->stdin_offset;
		ssize_t written = write(cgiInputFd, data, remaining);
		if (written > 0)
		{
			cgi->stdin_offset += static_cast<size_t>(written);
			currentConn->updateLastActivity();
		}
		else if (written < 0)
		{
			perror("write CGI stdin");
			close(cgiInputFd);
			currentConn->setCGIInputFd(-1);
			currentConn->setCGIInputClosed(true);
		}
	}
	if (cgi->stdin_offset >= cgi->stdin_buffer.size() && !currentConn->isCGIInputClosed())
	{
		close(cgiInputFd);
		currentConn->setCGIInputFd(-1);
		currentConn->setCGIInputClosed(true);
	}
}

void Engine::closeConnectionOrResetConnState(std::map<int, Connection*>::iterator& it, Connection* currentConn)
{
	if (currentConn->shouldClose())
	{
		destroyClientConnection(it);
		return;
	}
	currentConn->setRequestState(Connection::READING_HEADERS);
	currentConn->resetRequestStartTime();
	currentConn->setState(Connection::READING);
	++it;
}

void Engine::handleClientWrite(std::map<int, Connection*>::iterator& it, int clientFd, fd_set& writeSet)
{
	// get connection object
	Connection* currentConn = it->second;
	// check whether cliendfd is appeared in writeSet
	// if client is not ready to write, skip
	if (!FD_ISSET(clientFd, &writeSet))
	{
		++it;
		return;
	}

	// i fill the writeBuffer during request incoming phase
	// now i will send it out during this phase
	std::string& writebuffer = currentConn->getWriteBuffer();
	// write buffer might be empty before entering send()
	if (writebuffer.empty())
	{
		closeConnectionOrResetConnState(it, currentConn);
		return;
	}

	ssize_t sentByte = send(clientFd, writebuffer.data(), writebuffer.size(), 0);
	if (sentByte > 0)
	{
		writebuffer.erase(0, sentByte);
		currentConn->updateLastActivity();

		// just finish writing
		if (writebuffer.empty())
			closeConnectionOrResetConnState(it, currentConn);
		else
			++it;
		return;
	}
	// broken connection
	else if (sentByte == 0)
	{
		destroyClientConnection(it);
		return;
	}
	else if (sentByte < 0)
	{
		perror("send");
		destroyClientConnection(it);
		return;
	}

}

void Engine::processOutgoingData(fd_set& writeSet)
{
	// iterate the connection
	for (std::map<int, Connection*>::iterator it = _clientConnections.begin();
			it != _clientConnections.end();)
	{
		// get fd and pointer to connection object
		int clientFd = it->first;
		Connection* currentConn = it->second;

		// writing to the cgi stdin process
		if (currentConn->getState() == Connection::CGI_RUNNING)
			handleCGIStdinWrite(currentConn, writeSet);

		handleClientWrite(it, clientFd, writeSet);
	}
}
