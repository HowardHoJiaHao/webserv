/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   connection.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ho <hwai-keo@student.42kl.edu.my>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/21 07:48:52 by hwai-keo          #+#    #+#             */
/*   Updated: 2026/03/28 00:34:20 by ho               ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Connection.hpp"
#include <unistd.h>
#include <sys/types.h>
#include <ctime>

Connection::Connection(int fd)
	: _fd(fd),
	_readBuffer(),
	_writeBuffer(),
	_state(READING),
	_closed(false),
	_requestState(READING_HEADERS),
	_shouldClose(false),
	_lastActivity(std::time(NULL))
	{}

Connection::~Connection()
{
	if (!_closed)
		close();
}

int Connection::getFd() const
{
	return _fd;
}

std::string& Connection::getReadBuffer()
{
	return _readBuffer;
}

std::string& Connection::getWriteBuffer()
{
	return _writeBuffer;
}

void Connection::close()
{
	if(!_closed)
	{
		::close(_fd);
		_closed = true;
	}
}

Connection::RequestState Connection::getRequestState() const
{
	return _requestState;
}

void Connection::setRequestState(RequestState state)
{
	_requestState = state;
}

void Connection::appendToReadBuffer(const char* buffer, ssize_t bytes)
{
	if (bytes > 0)
		_readBuffer.append(buffer, bytes);
}

Connection::State Connection::getState() const
{
	return _state;
}

void Connection::setState(State state)
{
	_state = state;
}

void Connection::setShouldClose(bool value)
{
	_shouldClose = value;
}

bool Connection::shouldClose() const
{
	return _shouldClose;
}

void Connection::updateActivity()
{
	_lastActivity = std::time(NULL);
}

time_t Connection::getLastActivity() const
{
	return _lastActivity;
}