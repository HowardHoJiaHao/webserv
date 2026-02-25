/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   engine.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hwai-keo <hwai-keo@student.42kl.edu.my>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/19 20:53:32 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/02/25 18:30:08 by hwai-keo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ConfigFiles.hpp"
#include <map>
#include <utility>
#include <string>
#include "Connection.hpp"

#ifndef ENGINE_HPP
#define ENGINE_HPP

class Engine
{
	private:
		const ConfigFiles& _config;
		std::map<std::pair<std::string,int>, int> _listenSockets;
		std::map<int, Connection*> _connections;

		void registerListenSocketsForSelect(fd_set& readSet, int& maxFd);
		void registerClientSocketForSelect(fd_set& readSet, fd_set& writeSet, int& maxFd);
		void acceptPendingClientConnections(fd_set& readSet);
		void handleClientRequest(Connection* conn, const char* buffer, ssize_t bytes);
		void processIncomingData(fd_set& readSet);
		void processOutgoingData(fd_set& writeSet);

	public:
		Engine(const ConfigFiles& _config);
		~Engine();
		void	setupListeningSockets();
		void	run();
		std::string buildMinimalResponse();

};

#endif