/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hho-jia- <hho-jia-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/21 07:38:37 by hwai-keo          #+#    #+#             */
/*   Updated: 2026/04/14 15:22:08 by hho-jia-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONNECTION_HPP
# define CONNECTION_HPP

#include <string>
#include <sys/types.h> // ssize_t
#include "config/ServerConfig.hpp"

class Connection
{
	public:
		// READING: collect/parse request
		// CGI_RUNNING: wait/read CGI stdout
		// WRITING: send HTTP response to client
		enum State
		{
			READING,
			WRITING,
			CGI_RUNNING,
		};

		// runtime state container for cgi execution
		struct CGIContext
		{
			pid_t		pid; //pid 
			int			stdin_fd; // entering into cgi
			int			stdout_fd; // output fd
			bool		stdin_closed; // checking if closed
			std::string	stdin_buffer;
			size_t		stdin_offset;
			std::string	stdout_buffer;  // to store output buffer
			time_t		start_time;

			CGIContext()
				: pid(-1), stdin_fd(-1), stdout_fd(-1), stdin_closed(false),
				  stdin_buffer(), stdin_offset(0), stdout_buffer(), start_time(0){}
		};

		enum RequestState
		{
			READING_HEADERS,
			READING_BODY,
			COMPLETE
		};

		Connection(int fd);
		~Connection();



		std::string& getReadBuffer();
		std::string& getWriteBuffer();
		void setWriteBuffer(const std::string& buffer);
		void appendToWriteBuffer(const std::string& chunk);

		State getState() const;
		void setState(State state);

		void close();

		void appendToHeaderBuffer(const char* buffer, ssize_t bytes);

		RequestState getRequestState() const;
		void	setRequestState(RequestState state);
		void	setShouldClose(bool value);
		bool	shouldClose() const;
		void	setPendingSetCookieHeader(const std::string& header);
		const std::string& getPendingSetCookieHeader() const;
		void	clearPendingSetCookieHeader();

		void	updateLastActivity();
		time_t	getLastActivity() const;

		// ========		cgi in connection	==========
		// ===========		getter	==============
		CGIContext*			getCGI() const;
		int					getCGIPid() const;
		int					getCGIInputFd() const;
		int					getCGIOutputFd() const;
		bool				isCGIInputClosed() const;

		// ==========	setter	=============
		void				setCGI(CGIContext* cgi);
		void				setCGIPid(pid_t pid);
		void				setCGIInputFd(int fd);
		void				setCGIOutputFd(int fd);
		void				setCGIInputClosed(bool value);

		// =========	other	=============
		void				clearCGI();

		const ServerConfig* getServerConfig() const;
		void setServerConfig(const ServerConfig* config);

	private:
		int 				_fd;
		std::string 		_readBuffer;
		std::string 		_writeBuffer;  // this is to put proper http request return
		State				_state; // readin, writing, cgi_running
		bool				_closed;
		RequestState 		_requestState;
		bool				_shouldClose; // after response close flag
		std::string			_pendingSetCookieHeader; // cookie
		time_t				_lastActivity; // Last time that is updated

		// point to the struct object (cgi context)
		CGIContext*			_cgi;

		const ServerConfig* _serverConfig;
		

		Connection(const Connection&);
		Connection& operator=(const Connection&);


};

#endif