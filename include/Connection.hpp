/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hwai-keo <hwai-keo@student.42kl.edu.my>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/21 07:38:37 by hwai-keo          #+#    #+#             */
/*   Updated: 2026/02/22 02:42:38 by hwai-keo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONNECTION_HPP
# define CONNECTION_HPP

#include <string>

class Connection
{
		public:
		enum State
		{
			READING,
			WRITING,	
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

	private:
		int 		_fd;
		std::string _readBuffer;
		std::string _writeBuffer;
		State		_state;
		bool		_closed;

		

		Connection(const Connection&);
		Connection& operator=(const Connection&);


};

#endif