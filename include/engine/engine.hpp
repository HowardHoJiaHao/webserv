/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   engine.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hwai-keo <hwai-keo@student.42kl.edu.my>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/19 20:53:32 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/03/27 18:16:38 by hwai-keo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "ConfigFiles.hpp"
#include "Connection.hpp"
#include "httpHandling/httpRequest.hpp"

#include <map>
#include <string>
#include <utility>

#include <sys/select.h> // fd_set, FD_* macros
#include <sys/types.h>  // ssize_t

class Engine
{
	private:
		const ConfigFiles& _config;
		std::map<std::pair<std::string,int>, int> _listenSockets;
		std::map<int, Connection*> _clientConnections;

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
		std::string buildResponse(const std::string& status, const std::string& body, const std::string& contentType, bool shouldClose);
		std::string routeRequest(const HttpRequest& request);
		std::string handlePost(const HttpRequest& request);
};

#endif