/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   engineOutgoingData.cpp                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hwai-keo <hwai-keo@student.42kl.edu.my>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/03 17:23:36 by hwai-keo          #+#    #+#             */
/*   Updated: 2026/04/03 18:24:48 by hwai-keo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Webserv.hpp"

void Engine::processOutgoingData(fd_set& writeSet)
{
	for (std::map<int, Connection*>::iterator it = _clientConnections.begin();
			it != _clientConnections.end();)
	{
		int clientFd = it->first;
		Connection* currentConn = it->second;

		if (currentConn->getState() == Connection::CGI_RUNNING)
		{
			Connection::CGIContext* cgi = currentConn->getCGI();
			int cgi_in = currentConn->getCGIStdinFd();
			if (cgi != NULL && cgi_in != -1 && !currentConn->isCGIStdinClosed() && FD_ISSET(cgi_in, &writeSet))
			{
				if (cgi->stdin_offset < cgi->stdin_buffer.size())
				{
					const char* data = cgi->stdin_buffer.data() + cgi->stdin_offset;
					size_t remaining = cgi->stdin_buffer.size() - cgi->stdin_offset;
					ssize_t written = write(cgi_in, data, remaining);
					if (written > 0)
					{
						cgi->stdin_offset += static_cast<size_t>(written);
						currentConn->updateActivity();
					}
					else if (written < 0)
					{
						if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EPIPE)
						{
							perror("write CGI stdin");
							close(cgi_in);
							currentConn->setCGIStdinFd(-1);
							currentConn->setCGIStdinClosed(true);
						}
						else if (errno == EPIPE)
						{
							close(cgi_in);
							currentConn->setCGIStdinFd(-1);
							currentConn->setCGIStdinClosed(true);
						}
					}
				}
				if (cgi->stdin_offset >= cgi->stdin_buffer.size() && !currentConn->isCGIStdinClosed())
				{
					close(cgi_in);
					currentConn->setCGIStdinFd(-1);
					currentConn->setCGIStdinClosed(true);
				}
			}
		}

		if (!FD_ISSET(clientFd, &writeSet))
		{
			++it;
			continue;
		}

		std::string& writebuffer = currentConn->getWriteBuffer();
		if (writebuffer.empty())
		{
			if (currentConn->shouldClose())
			{
				delete currentConn;
				_clientListenEndpoints.erase(clientFd);
				_clientConnections.erase(it++);
				continue;
			}

			currentConn->setRequestState(Connection::READING_HEADERS);
			currentConn->setState(Connection::READING);
			++it;
			continue;
		}

		ssize_t sentByte = send(clientFd, writebuffer.data(), writebuffer.size(), 0);
		if (sentByte > 0)
		{
			writebuffer.erase(0, sentByte);
			currentConn->updateActivity();

			if (writebuffer.empty())
			{
				if (currentConn->shouldClose())
				{
					delete currentConn;
					_clientListenEndpoints.erase(clientFd);
					_clientConnections.erase(it++);
					continue;
				}
				currentConn->setRequestState(Connection::READING_HEADERS);
				currentConn->setState(Connection::READING);
			}
			++it;
			continue;
		}
		if (sentByte == 0)
		{
			delete currentConn;
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
		delete currentConn;
		_clientListenEndpoints.erase(clientFd);
		_clientConnections.erase(it++);
	}
}
