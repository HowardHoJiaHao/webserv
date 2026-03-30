/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ho <hwai-keo@student.42kl.edu.my>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/21 07:38:37 by hwai-keo          #+#    #+#             */
/*   Updated: 2026/03/31 01:50:45 by ho               ###   ########.fr       */
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
			CGI_RUNNING,
		};

		struct CGIContext
		{
			pid_t pid;
			int stdin_fd;
			int stdout_fd;
			bool stdin_closed;

			CGIContext()
				: pid(-1), stdin_fd(-1), stdout_fd(-1), stdin_closed(false){}
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

		CGIContext* getCGI() const;
		int	getCGIPid() const;
		int getCGIStdinFd() const;
		int getCGIStdoutFd() const;
		bool isCGIStdinClosed() const;

		void setCGI(CGIContext* cgi);
		void setCGIPid(pid_t pid);
		void setCGIStdinFd(int fd);
		void setCGIStdoutFd(int fd);
		void setCGIStdinClosed(bool value);
		void clearCGI();

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

		CGIContext*	_cgi;


};

#endif