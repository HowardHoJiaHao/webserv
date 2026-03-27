/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ho <hwai-keo@student.42kl.edu.my>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/21 07:38:37 by hwai-keo          #+#    #+#             */
/*   Updated: 2026/03/28 00:34:20 by ho               ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONNECTION_HPP
# define CONNECTION_HPP

#include <string>
#include <sys/types.h> // ssize_t

class Connection
{
		public:
		enum State
		{
			READING,
			WRITING,	
		};

		enum RequestState
		{
			READING_HEADERS,
			READING_BODY,
			COMPLETE
		};

		Connection(int fd);
		~Connection();



		int getFd() const;
		std::string& getReadBuffer();
		std::string& getWriteBuffer();

		State getState() const;
		void setState(State state);

		void close();

		void appendToReadBuffer(const char* buffer, ssize_t bytes);

		RequestState getRequestState() const;
		void	setRequestState(RequestState state);
		void	setShouldClose(bool value);
		bool	shouldClose() const;

		void	updateActivity();
		time_t	getLastActivity() const;

	private:
		int 		_fd;
		std::string _readBuffer;
		std::string _writeBuffer;
		State		_state;
		bool		_closed;
		RequestState _requestState;
		bool		_shouldClose;
		time_t		_lastActivity;

		

		Connection(const Connection&);
		Connection& operator=(const Connection&);


};

#endif