#include "engine.hpp"
#include "FileHandler.hpp"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <sstream>

static std::string trimAsciiCgi(const std::string& value)
{
	size_t start = 0;
	while (start < value.size() && (value[start] == ' ' || value[start] == '\t'))
		++start;

	size_t end = value.size();
	while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\t'))
		--end;

	return value.substr(start, end - start);
}

static bool isDigitsOnlyCgi(const std::string& value)
{
	if (value.empty())
		return false;
	for (size_t i = 0; i < value.size(); ++i)
	{
		if (value[i] < '0' || value[i] > '9')
			return false;
	}
	return true;
}

static std::string normalizeHeaderKeyForEnvCgi(const std::string& key)
{
	std::string normalized;
	normalized.reserve(key.size());

	for (size_t i = 0; i < key.size(); ++i)
	{
		char c = key[i];
		if (c >= 'a' && c <= 'z')
			normalized.push_back(static_cast<char>(c - 'a' + 'A'));
		else if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
			normalized.push_back(c);
		else
			normalized.push_back('_');
	}

	return normalized;
}

static void parseHostAndPortFromHeaderCgi(const std::string& hostHeaderValue, std::string& serverName, std::string& serverPort)
{
	// trim front and back of the string
	std::string value = trimAsciiCgi(hostHeaderValue);
	if (value.empty())
		return;

	// Solve this example [::1]:8080
	if (value[0] == '[')
	{
		size_t bracketEnd = value.find(']');
		if (bracketEnd != std::string::npos)
		{
			serverName = value.substr(1, bracketEnd - 1);
			if (bracketEnd + 1 < value.size() && value[bracketEnd + 1] == ':')
			{
				std::string parsedPort = value.substr(bracketEnd + 2);
				if (isDigitsOnlyCgi(parsedPort))
					serverPort = parsedPort;
			}
			return;
		}
	}

	size_t firstColon = value.find(':');
	size_t lastColon = value.rfind(':');
	if (firstColon != std::string::npos && firstColon == lastColon)
	{
		std::string hostPart = value.substr(0, firstColon);
		std::string portPart = value.substr(firstColon + 1);
		if (!hostPart.empty())
			serverName = hostPart;
		if (isDigitsOnlyCgi(portPart))
			serverPort = portPart;
		return;
	}

	serverName = value;
}

// static std::string buildRequestUriCgi(const HttpRequest& request)
// {
// 	if (request.getQuery().empty())
// 		return request.getPath();
// 	return request.getPath() + "?" + request.getQuery();
// }

static void appendForwardedHeaderVarsCgi(const HttpRequest& request, std::vector<std::string>& envStrings)
{
	const std::map<std::string, std::string>& headers = request.getHeaders();
	for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it)
	{
		if (it->first == "content-type" || it->first == "content-length")
			continue;

		std::string envKey = "HTTP_" + normalizeHeaderKeyForEnvCgi(it->first);
		envStrings.push_back(envKey + "=" + it->second);
	}
}

bool	Engine::resolveCGIScriptPath(Connection* conn, const HttpRequest& request, const ServerConfig& serverConfig, const LocationConfig* location, std::string& scriptPath)
{

	if (hasPathTraversal(request.getPath()))
	{
		conn->setShouldClose(true);
		conn->setWriteBuffer(buildErrorResponse(403, true, &serverConfig));
		return false;
	}

	std::string root = resolveLocationRoot(serverConfig, location);
	std::string mappedPath = mapRequestPathForLocationRoot(request.getPath(), location);
	scriptPath = root + mappedPath;
	return true;
}

bool	Engine::validateCGIScript(Connection* conn, const std::string& scriptPath, const ServerConfig& serverConfig, bool shouldClose)
{
	// Check for file Exists
	if (!FileHandler::fileExists(scriptPath))
	{
		conn->setShouldClose(shouldClose);
		conn->setWriteBuffer(buildErrorResponse(404, conn->shouldClose(), &serverConfig));
		return false;
	}
	{ // create a local scope
		struct stat st;
		// stat putting the scriptPath into struct 
		if (stat(scriptPath.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
		{
			// S_ISREG check for regular file like script.py and delete.pl
			// not regular file , directory , socket , device , pipe
			conn->setShouldClose(shouldClose);
			conn->setWriteBuffer(buildErrorResponse(404, conn->shouldClose(), &serverConfig));
			return false;
		}
	}

	// Check if the file is executable
	if (access(scriptPath.c_str(), X_OK) != 0)
	{
		conn->setShouldClose(true);
		conn->setWriteBuffer(buildErrorResponse(403, true, &serverConfig));
		return false;
	}

	return true;
}

bool	Engine::createCGIProcess(Connection* conn, const ServerConfig& serverConfig, int in_pipe[2], int out_pipe[2], pid_t& pid)
{
	if (pipe(in_pipe) < 0)
	{
		perror("pipe in_pipe failed");
		conn->setShouldClose(true);
		conn->setWriteBuffer(buildErrorResponse(500, true, &serverConfig));
		return false;
	}

	if (pipe(out_pipe) < 0)
	{
		perror("pipe out_pip failed");
		close(in_pipe[0]);
		close(in_pipe[1]);
		conn->setShouldClose(true);
		conn->setWriteBuffer(buildErrorResponse(500, true, &serverConfig));
		return false;
	}
	pid = fork();
	if (pid < 0)
	{
		perror("fork failed");
		close(in_pipe[0]);
		close(in_pipe[1]);
		close(out_pipe[0]);
		close(out_pipe[1]);
		conn->setShouldClose(true);
		conn->setWriteBuffer(buildErrorResponse(500, true, &serverConfig));
		return false;
	}

	return true;
}

void	Engine::setupCGIChildProcess(int in_pipe[2], int out_pipe[2], const std::string& scriptPath, const HttpRequest& request, const ServerConfig& serverConfig)
{
	if (dup2(in_pipe[0], STDIN_FILENO) < 0)
	{
		perror("dup2 stdin failed");
		_exit(1);
	}
	if (dup2(out_pipe[1], STDOUT_FILENO) < 0)
	{
		perror("dup2 stdout failed");
		_exit(1);
	}
	close(in_pipe[0]);
	close(in_pipe[1]);
	close(out_pipe[0]);
	close(out_pipe[1]);

	std::string executablePath = scriptPath;
	size_t slashPos = scriptPath.find_last_of('/');
	if (slashPos != std::string::npos)
	{
		std::string scriptDir = (slashPos == 0) ? "/" : scriptPath.substr(0, slashPos);
		// child changes current working directory to script folder.
		// Safer and will not take other file incase
		if (chdir(scriptDir.c_str()) != 0)
		{
			perror("chdir failed");
			_exit(1);
		}
		executablePath = scriptPath.substr(slashPos + 1);
		if (executablePath.empty())
		{
			perror("invalid CGI script path");
			_exit(1);
		}
	}

	// Preparing Server Name , Server Port 
	std::string serverName = serverConfig.getHost();
	if (serverName.empty())
		serverName = "0.0.0.0";
	std::ostringstream serverPortOss;
	serverPortOss << serverConfig.getPort();
	std::string serverPort = serverPortOss.str();

	// This is a fallback to prevent empty, bad port , weird/ multiple saperator (Host: example.com:80:90)
	const std::string* hostHeader = request.getHeader("host");
	if (hostHeader != NULL && !hostHeader->empty())
		parseHostAndPortFromHeaderCgi(*hostHeader, serverName, serverPort);

	

	std::vector<std::string> envStrings;
	envStrings.push_back("REQUEST_METHOD=" + request.getMethod()); // ex: GET, POST
	envStrings.push_back("QUERY_STRING=" + request.getQuery()); // ex: page=2&sort=asc

	// envStrings.push_back("REQUEST_URI=" + buildRequestUriCgi(request)); // ex: /cgi-bin/form.py?page=2&sort=asc
	// envStrings.push_back("SCRIPT_NAME=" + request.getPath()); // ex: /cgi-bin/form.py
	// envStrings.push_back("SCRIPT_FILENAME=" + scriptPath); // ex: ./www1/cgi-bin/form.py
	// envStrings.push_back("PATH_INFO=" + request.getPath()); // ex: /cgi-bin/form.py
	// envStrings.push_back("PATH_TRANSLATED=" + scriptPath); // ex: ./www1/cgi-bin/form.py
	// envStrings.push_back("SERVER_PROTOCOL=" + request.getVersion()); // ex: HTTP/1.1
	// envStrings.push_back("SERVER_SOFTWARE=webserv/1.0"); // server signature
	// envStrings.push_back("GATEWAY_INTERFACE=CGI/1.1"); // CGI spec version
	// envStrings.push_back("SERVER_NAME=" + serverName); // ex: localhost
	// envStrings.push_back("SERVER_PORT=" + serverPort); // ex: 8080
	// envStrings.push_back("DOCUMENT_ROOT=" + serverConfig.getRoot()); // ex: ./www1
	// envStrings.push_back("REDIRECT_STATUS=200"); // compatibility for some CGI runtimes
	

	std::ostringstream contentLength;
	contentLength << request.getBody().size();
	envStrings.push_back("CONTENT_LENGTH=" + contentLength.str());

	const std::string* contentTypeHeader = request.getHeader("content-type");
	if (contentTypeHeader != NULL && !contentTypeHeader->empty())
		envStrings.push_back("CONTENT_TYPE=" + *contentTypeHeader);
	else if (request.getMethod() == "POST" || request.getMethod() == "GET") 
		envStrings.push_back("CONTENT_TYPE=application/octet-stream");
	// so POST usualy need to send value like name , age those so it need default content type for the cgi script to read where GET wants value and there will be no value inside body 
	
	// Just to add prefix HTTP but we did not use 
	appendForwardedHeaderVarsCgi(request, envStrings);

	std::vector<char*> envp;
	for (size_t i = 0; i < envStrings.size(); ++i)
		envp.push_back(const_cast<char*>(envStrings[i].c_str()));
	envp.push_back(NULL);

	char* argv[] = { const_cast<char*>(executablePath.c_str()), NULL};

	execve(executablePath.c_str(), argv, &envp[0]);
	// Example
	// const char* argv[] = { "ls", "-l", "/home", NULL };
	// const char* envp[] = { "PATH=/usr/bin", "HOME=/root", NULL };
	// execve("/bin/ls", argv, envp);

	perror("execve failed");
	_exit(1);
}

void	Engine::setupCGIParent(Connection* conn, const HttpRequest& request, int in_pipe[2], int out_pipe[2], pid_t pid, bool shouldClose)
{
	close(in_pipe[0]);
	close(out_pipe[1]);

	Connection::CGIContext* cgi = new Connection::CGIContext();
	cgi->stdin_closed = false;
	cgi->stdin_buffer = request.getBody();
	cgi->stdin_offset = 0;
	cgi->stdout_buffer.clear();
	cgi->start_time = std::time(NULL);

	conn->setCGI(cgi);
	conn->setCGIPid(pid);
	conn->setCGIInputFd(in_pipe[1]);
	conn->setCGIOutputFd(out_pipe[0]);

	// enable non-blocking mode.
	// Non-blocking means: do not wait.

	// For a fd in non-blocking mode:

	// read() returns immediately.
	// write() returns immediately.
	// If operation cannot proceed now, it returns error (-1, usually EAGAIN/EWOULDBLOCK) instead of pausing your program.
	// Blocking mode means:

	// read() may wait until data arrives.
	// write() may wait until buffer has space.
	// Your whole single-thread event loop can freeze during that wait.
	if (conn->getCGIInputFd() != -1 && fcntl(conn->getCGIInputFd(), F_SETFL, O_NONBLOCK) == -1)
	{
		close(conn->getCGIInputFd());
		conn->setCGIInputFd(-1);
		conn->setCGIInputClosed(true);
	}
	if (conn->getCGIOutputFd() != -1 && fcntl(conn->getCGIOutputFd(), F_SETFL, O_NONBLOCK) == -1)
	{
		close(conn->getCGIOutputFd());
		conn->setCGIOutputFd(-1);
	}

	if (cgi->stdin_buffer.empty())
	{
		if (conn->getCGIInputFd() != -1)
			close(conn->getCGIInputFd());
		conn->setCGIInputFd(-1);
		conn->setCGIInputClosed(true);
	}

	conn->setShouldClose(shouldClose);
	conn->setState(Connection::CGI_RUNNING);
	conn->updateLastActivity();
}

bool	Engine::launchCGI(Connection* conn, const HttpRequest& request, const ServerConfig& serverConfig, const LocationConfig* location, bool shouldClose)
{
	std::string scriptPath;

	// Resolve trancersal path ( .. / %2e%2e)
	// Get final script path in scriptPath
	if (!resolveCGIScriptPath(conn, request, serverConfig, location, scriptPath))
		return false;

	// validate if script exist / executable
	if (!validateCGIScript(conn, scriptPath, serverConfig, shouldClose))
		return false;

	// create pipe and fork 
	int in_pipe[2];
	int out_pipe[2];
	pid_t pid;

	// pipe checking and create fork 
	if (!createCGIProcess(conn, serverConfig, in_pipe, out_pipe, pid))
		return false;

	if (pid == 0)
	{
		// Pipe redirection
		// child process and execution
		setupCGIChildProcess(in_pipe, out_pipe, scriptPath, request, serverConfig);
		_exit(1);
	}

	// parent setup nonblocking  and i/o state
	setupCGIParent(conn, request, in_pipe, out_pipe, pid, shouldClose);
	return true;
}
