/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   engine.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: Ho Wai Keong <hwai_keo@student.42kl.edu    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/19 20:53:32 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/04/05 11:18:07 by Ho Wai Keon      ###   ########.fr       */
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

		// ========			engineIncomingData.cpp			==========

		void	processIncomingData(fd_set& readSet);
		
		void	handleClientSocketRead(std::map<int, Connection*>::iterator& it, int clientFd, fd_set& readSet);
		bool	processCGIOutput(Connection* currentConn, fd_set& readSet);
		void	handleCGIStdinWrite(Connection* conn, fd_set& writeSet);
		void	handleClientWrite(std::map<int, Connection*>::iterator& it, int clientFd, fd_set& writeSet);
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

		// ===================				handleClientRequest		========================
		
		void handleClientRequest(Connection* conn, const char* buffer, ssize_t bytes);

		bool prepareConnection(Connection* conn, const char* buffer, ssize_t bytes, size_t& headerEnd);
		bool processBufferedRequests(Connection* conn, bool& producedResponse);
		bool enforceRequestSizeLimits(Connection* conn, size_t headerEnd);
		bool handleRequestExtraction(Connection* conn, std::string& rawRequest, bool& extracted);
		bool handleRequestParsing(Connection* conn, const std::string& rawRequest, HttpRequest& request, const ServerConfig* defaultServer);
		bool handleRequestValidation(Connection* conn, const HttpRequest& request, const ServerConfig* serverConfig);
		void handleSession(const HttpRequest& request, std::vector<std::string>& extraHeaders);
		bool handleRequestExecution(Connection* conn, const HttpRequest& request, const ServerConfig* defaultServer, const ServerConfig* serverConfig, bool shouldClose, bool& producedResponse);
		std::string handleGet(const HttpRequest& request, bool shouldClose, const ServerConfig& serverConfig, const LocationConfig* location);

		// ==================== 			build response			========================

		std::string buildResponse(const std::string& status, const std::string& body, const std::string& contentType, bool shouldClose, const std::vector<std::string>& extraHeaders);
		
		std::string buildStandardResponse(int code, const std::string& body, const std::string& contentType, bool shouldClose);
		std::string buildRedirectResponse(int code, const std::string& target, bool shouldClose);
		std::string buildErrorResponse(int code, bool shouldClose, const ServerConfig* serverConfig);
		std::string buildErrorResponse(int code, bool shouldClose, const ServerConfig* serverConfig, const std::vector<std::string>& extraHeaders);

		// ====================				launchCGI				========================
		
		bool		resolveCGIScriptPath(Connection* conn, const HttpRequest& request, const ServerConfig& serverConfig, bool shouldClose, std::string& scriptPath);
		bool		validateCGIScript(Connection* conn, const std::string& scriptPath, const ServerConfig& serverConfig, bool shouldClose);
		bool		createCGIProcess(Connection* conn, const ServerConfig& serverConfig, int in_pipe[2], int out_pipe[2], pid_t& pid);
		void		setupCGIChildProcess(int in_pipe[2], int out_pipe[2], const std::string& scriptPath, const HttpRequest& request);
		void		setupCGIParent(Connection* conn, const HttpRequest& request, int in_pipe[2], int out_pipe[2], pid_t pid, bool shouldClose);

	public:
		Engine(const ConfigFiles& _config);
		~Engine();

		void		setupListeningSockets();
		void		run();
		std::string routeRequest(const HttpRequest& request, bool shouldClose, const ServerConfig& serverConfig);
		std::string handlePost(const HttpRequest& request, bool shouldClose, const ServerConfig& serverConfig, const LocationConfig* location);
		std::string handleDelete(const HttpRequest& request, bool shouldClose, const ServerConfig& serverConfig);
		bool 		launchCGI(Connection* conn, const HttpRequest& request, const ServerConfig& serverConfig, bool shouldClose);

};

#endif