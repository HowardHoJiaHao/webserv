/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   engine.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ho <hwai-keo@student.42kl.edu.my>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/19 20:53:32 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/03/23 02:53:29 by ho               ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ConfigFiles.hpp"
#include <map>
#include <utility>
#include <string>
#include "Connection.hpp"
#include "httpHandling/httpRequest.hpp"

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
		std::string build405Response();
		std::string buildIndexResponse();
		std::string build404Response();
		std::string build400Response();
		std::string buildResponse(const std::string& status, const std::string& body, const std::string& contentType);
		std::string routeRequest(const HttpRequest& request);

};

#endif