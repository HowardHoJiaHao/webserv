/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   connection.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ho <hwai-keo@student.42kl.edu.my>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/21 07:48:52 by hwai-keo          #+#    #+#             */
/*   Updated: 2026/03/31 02:03:49 by ho               ###   ########.fr       */
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
	_lastActivity(std::time(NULL)),
	_cgi(NULL)
	{}

Connection::~Connection()
{
	clearCGI();
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
		clearCGI();
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

Connection::CGIContext* Connection::getCGI() const
{
	return _cgi;
}

int Connection::getCGIPid() const
{
	if (!_cgi)
	{
		return -1;
	}
	return _cgi->pid;
}

int Connection::getCGIStdinFd() const
{
	if (!_cgi)
		return -1;
	return _cgi->stdin_fd;
}

int Connection::getCGIStdoutFd() const
{
	if (!_cgi)
	{
		return -1;
	}
	return _cgi->stdout_fd;
}

bool Connection::isCGIStdinClosed() const
{
	if (!_cgi)
		return true;
	return _cgi->stdin_closed;
}

void Connection::setCGI(CGIContext* cgi)
{
	if (_cgi)
		clearCGI();
	_cgi = cgi;
}

void Connection::setCGIPid(pid_t pid)
{
	if (_cgi)
		_cgi->pid = pid;
}

void Connection::setCGIStdinFd(int fd)
{
	if (_cgi)
		_cgi->stdin_fd = fd;
}

void Connection::setCGIStdoutFd(int fd)
{
	if (_cgi)
		_cgi->stdout_fd = fd;
}

void Connection::setCGIStdinClosed(bool value)
{
	if (_cgi)
		_cgi->stdin_closed = value;
}

void Connection::clearCGI()
{
	if (!_cgi)
		return;

	if (_cgi->stdin_fd != -1)
	{
		::close(_cgi->stdin_fd);
		_cgi->stdin_fd = -1;
	}
	if (_cgi->stdout_fd != -1)
	{
		::close(_cgi->stdout_fd);
		_cgi->stdout_fd = -1;
	}
	delete _cgi;
	_cgi = NULL;
}

