/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   engine.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: Ho Wai Keong <hwai_keo@student.42kl.edu    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/19 20:53:32 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/04/05 00:02:48 by Ho Wai Keon      ###   ########.fr       */
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
		const ConfigFiles& 								_config;
		std::map<std::pair<std::string,int>, int> 		_listenSockets;
		std::map<int, Connection*> 						_clientConnections; // <- client socket + state + buffer
		std::map<int, std::pair<std::string, int> > 	_clientListenEndpoints;
		std::map<std::string, int> 						_sessions;

		void registerListenSocketsForSelect(fd_set& readSet, int& maxFd);
		void registerClientSocketForSelect(fd_set& readSet, fd_set& writeSet, int& maxFd);
		void acceptPendingClientConnections(fd_set& readSet);
		void handleClientRequest(Connection* conn, const char* buffer, ssize_t bytes);

		// ========			engineIncomingData.cpp			==========

		void	processIncomingData(fd_set& readSet);
		void	handleClientSocketRead(std::map<int, Connection*>::iterator& it, int clientFd, fd_set& readSet);
		bool	processCGIOutput(Connection* currentConn, fd_set& readSet);
		void	parseCGIHeaders(const std::string& headerSection, std::string& status, std::string& contentType, std::vector<std::string>& extraHeaders);
		void	buildResponseFromCGIOutput(Connection* currentConn, const std::string& cgiOutput);
		bool	handleCGIWouldBlock(Connection* currentConn, struct timeval& startTime);
		void	handleCGIReadError(Connection* currentConn);

		void processOutgoingData(fd_set& writeSet);
		void checkTimeouts();

		const ServerConfig* findServerConfig(const std::string& host, int port) const;
		//const ServerConfig* findServerConfigForConnection(int clientFd) const;
		const LocationConfig* findBestLocation(const ServerConfig& serverConfig, const std::string& path) const;
		bool isMethodAllowed(const std::string& method, const LocationConfig* location) const;
		std::string buildErrorResponse(int code, bool shouldClose, const ServerConfig* serverConfig);
		std::string buildErrorResponse(int code, bool shouldClose, const ServerConfig* serverConfig, const std::vector<std::string>& extraHeaders);

	public:
		Engine(const ConfigFiles& _config);
		~Engine();
		void	setupListeningSockets();
		void	run();
		std::string buildResponse(const std::string& status, const std::string& body, const std::string& contentType, bool shouldClose, const std::vector<std::string>& extraHeaders);
		std::string routeRequest(const HttpRequest& request, bool shouldClose, const ServerConfig& serverConfig);
		std::string handlePost(const HttpRequest& request, bool shouldClose, const ServerConfig& serverConfig, const LocationConfig* location);
		std::string handleDelete(const HttpRequest& request, bool shouldClose, const ServerConfig& serverConfig);

		bool 		launchCGI(Connection* conn, const HttpRequest& request, const ServerConfig& serverConfig, bool shouldClose);

};

#endif