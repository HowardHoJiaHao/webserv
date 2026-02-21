/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   connection.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hwai-keo <hwai-keo@student.42kl.edu.my>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/21 07:48:52 by hwai-keo          #+#    #+#             */
/*   Updated: 2026/02/21 10:21:14 by hwai-keo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Connection.hpp"
#include <unistd.h>

Connection::Connection(int fd)
	: _fd(fd),
	_readBuffer(),
	_writeBuffer(),
	_closed(false)
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