/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   engineOutgoingData.cpp                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hwai-keo <hwai-keo@student.42kl.edu.my>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/03 17:23:36 by hwai-keo          #+#    #+#             */
/*   Updated: 2026/04/08 18:25:51 by hwai-keo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Webserv.hpp"

void Engine::handleCGIStdinWrite(Connection* currentConn, fd_set& writeSet)
{
	Connection::CGIContext* cgi = currentConn->getCGI();
	int cgi_in = currentConn->getCGIStdinFd();

	if (cgi == NULL || cgi_in == -1 || currentConn->isCGIStdinClosed() || !FD_ISSET(cgi_in, &writeSet))
		return;

	if (cgi->stdin_offset < cgi->stdin_buffer.size())
	{
		const char* data = cgi->stdin_buffer.data() + cgi->stdin_offset;
		size_t remaining = cgi->stdin_buffer.size() - cgi->stdin_offset;
		ssize_t written = write(cgi_in, data, remaining);
		if (written > 0)
		{
			cgi->stdin_offset += static_cast<size_t>(written);
			currentConn->updateLastActivity();
		}
		else if (written < 0)
		{
			perror("write CGI stdin");
			close(cgi_in);
			currentConn->setCGIStdinFd(-1);
			currentConn->setCGIStdinClosed(true);
		}
	}
	if (cgi->stdin_offset >= cgi->stdin_buffer.size() && !currentConn->isCGIStdinClosed())
	{
		close(cgi_in);
		currentConn->setCGIStdinFd(-1);
		currentConn->setCGIStdinClosed(true);
	}
}

void Engine::handleClientWrite(std::map<int, Connection*>::iterator& it, int clientFd, fd_set& writeSet)
{
	Connection* currentConn = it->second;

	if (!FD_ISSET(clientFd, &writeSet))
	{
		++it;
		return;
	}

	std::string& writebuffer = currentConn->getWriteBuffer();
	if (writebuffer.empty())
	{
		if (currentConn->shouldClose())
		{
			destroyClientConnection(it);
			return;
		}

		currentConn->setRequestState(Connection::READING_HEADERS);
		currentConn->setState(Connection::READING);
		++it;
		return;
	}

	ssize_t sentByte = send(clientFd, writebuffer.data(), writebuffer.size(), 0);
	if (sentByte > 0)
	{
		writebuffer.erase(0, sentByte);
		currentConn->updateLastActivity();

		if (writebuffer.empty())
		{
			if (currentConn->shouldClose())
			{
				destroyClientConnection(it);
				return;
			}
			currentConn->setRequestState(Connection::READING_HEADERS);
			currentConn->setState(Connection::READING);
		}
		++it;
		return;
	}
	if (sentByte == 0)
	{
		destroyClientConnection(it);
		return;
	}
	perror("send");
	destroyClientConnection(it);
}

void Engine::processOutgoingData(fd_set& writeSet)
{
	for (std::map<int, Connection*>::iterator it = _clientConnections.begin();
			it != _clientConnections.end();)
	{
		int clientFd = it->first;
		Connection* currentConn = it->second;

		if (currentConn->getState() == Connection::CGI_RUNNING)
			handleCGIStdinWrite(currentConn, writeSet);

		handleClientWrite(it, clientFd, writeSet);
	}
}
