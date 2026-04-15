/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   engine.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hho-jia- <hho-jia-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/19 20:53:32 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/04/15 09:27:01 by hho-jia-         ###   ########.fr       */
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
		std::map<std::string, time_t>					_sessions;	// <- should be global rather than per connection, because session survive when connection gone
		unsigned long									_sessionCounter;

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
		void	handleCGIReadError(Connection* currentConn);

		void processOutgoingData(fd_set& writeSet);
		void closeConnectionOrResetConnState(std::map<int, Connection*>::iterator& it, Connection* currentConn);
		void checkTimeouts();
		void cleanup();

		const ServerConfig* findServerConfig(const std::string& host, int port) const;
		void destroyClientConnection(std::map<int, Connection*>::iterator& it);
		const LocationConfig* findBestLocation(const ServerConfig& serverConfig, const std::string& path) const;
		std::string resolveLocationRoot(const ServerConfig& serverConfig, const LocationConfig* location) const;
		std::string mapRequestPathForLocationRoot(const std::string& requestPath, const LocationConfig* location) const;
		bool hasPathTraversal(const std::string& path) const;
		bool isMethodAllowed(const std::string& method, const LocationConfig* location) const;
		std::vector<std::string> methodNotAllowedHeaders(const LocationConfig* location) const;

		// ===================				handleClientRequest		========================
		
		void handleClientRequest(Connection* conn, const char* buffer, ssize_t bytes);

		bool attemptIncomingHeader(Connection* conn, const char* buffer, ssize_t bytes, size_t& headerEnd);
		bool processBufferedRequests(Connection* conn, bool& producedResponse);
		bool enforceRequestSizeLimits(Connection* conn, size_t headerEnd);
		bool handleRequestExtraction(Connection* conn, std::string& rawRequest, bool& extracted);
		bool handleRequestParsing(Connection* conn, const std::string& rawRequest, HttpRequest& request, const ServerConfig* serverConfig);
		bool handleRequestExecution(Connection* conn, const HttpRequest& request, const ServerConfig* serverConfig, const LocationConfig* location, bool shouldClose, bool& producedResponse);
		std::string handleGet(const HttpRequest& request, bool shouldClose, const ServerConfig& serverConfig, const LocationConfig* location);

		// ==================== 			build response			========================

		std::string buildResponse(const std::string& status, const std::string& body, const std::string& contentType, bool shouldClose, const std::vector<std::string>& extraHeaders);
		
		std::string buildStandardResponse(int code, const std::string& body, const std::string& contentType, bool shouldClose);
		std::string buildRedirectResponse(int code, const std::string& target, bool shouldClose);
		std::string buildErrorResponse(int code, bool shouldClose, const ServerConfig* serverConfig);
		std::string buildErrorResponse(int code, bool shouldClose, const ServerConfig* serverConfig, const std::vector<std::string>& extraHeaders);
		std::string appendHeaderToResponse(const std::string& response, const std::string& headerLine) const;

		void		pruneExpiredSessions(time_t now);
		std::string generateSessionId(time_t now);
		std::string ensureSessionCookieHeader(const HttpRequest& request);

		// ====================				launchCGI				========================
		
		bool		resolveCGIScriptPath(Connection* conn, const HttpRequest& request, const ServerConfig& serverConfig, const LocationConfig* location, std::string& scriptPath);
		bool		validateCGIScript(Connection* conn, const std::string& scriptPath, const ServerConfig& serverConfig, bool shouldClose);
		bool		createCGIProcess(Connection* conn, const ServerConfig& serverConfig, int in_pipe[2], int out_pipe[2], pid_t& pid);
		void		setupCGIChildProcess(int in_pipe[2], int out_pipe[2], const std::string& scriptPath, const HttpRequest& request, const ServerConfig& serverConfig);
		void		setupCGIParent(Connection* conn, const HttpRequest& request, int in_pipe[2], int out_pipe[2], pid_t pid, bool shouldClose);

	public:
		Engine(const ConfigFiles& _config);
		~Engine();

		void		setupListeningSockets();
		void		run();
		std::string routeRequest(const HttpRequest& request, bool shouldClose, const ServerConfig& serverConfig, const LocationConfig* location);
		std::string handlePost(const HttpRequest& request, bool shouldClose, const ServerConfig& serverConfig, const LocationConfig* location);
		std::string handleDelete(const HttpRequest& request, bool shouldClose, const ServerConfig& serverConfig, const LocationConfig* location);
		bool 		launchCGI(Connection* conn, const HttpRequest& request, const ServerConfig& serverConfig, const LocationConfig* location, bool shouldClose);

};

#endif