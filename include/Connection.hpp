/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hwai-keo <hwai-keo@student.42kl.edu.my>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/21 07:38:37 by hwai-keo          #+#    #+#             */
/*   Updated: 2026/02/21 10:21:08 by hwai-keo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONNECTION_HPP
# define CONNECTION_HPP

#include <string>

class Connection
{
	private:
		int 		_fd;
		std::string _readBuffer;
		std::string _writeBuffer;
		bool		_closed;

		Connection(const Connection&);
		Connection& operator=(const Connection&);
	public:
		Connection(int fd);
		~Connection();

		int getFd() const;
		std::string& getReadBuffer();
		std::string& getWriteBuffer();
		void close();
};

#endif