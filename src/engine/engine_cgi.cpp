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

bool	Engine::launchCGI(Connection* conn, const HttpRequest& request, const ServerConfig& serverConfig, bool shouldClose)
{
	if (request.getPath().find("..") != std::string::npos)
	{
		conn->setShouldClose(true);
		conn->getWriteBuffer() = buildErrorResponse(403, "Forbidden", true, &serverConfig);
		return false;
	}

	std::string root = serverConfig.getRoot();
	const LocationConfig* location = findBestLocation(serverConfig, request.getPath());
	if (location != NULL && !location->getRoot().empty())
		root = location->getRoot();

	std::string scriptPath = root + request.getPath();
	if (!FileHandler::fileExists(scriptPath))
	{
		conn->setShouldClose(shouldClose);
		conn->getWriteBuffer() = build404Response(conn->shouldClose(), &serverConfig);
		return false;
	}
	{
		struct stat st;
		if (stat(scriptPath.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
		{
			conn->setShouldClose(shouldClose);
			conn->getWriteBuffer() = build404Response(conn->shouldClose(), &serverConfig);
			return false;
		}
	}
	if (access(scriptPath.c_str(), X_OK) != 0)
	{
		conn->setShouldClose(true);
		conn->getWriteBuffer() = buildErrorResponse(403, "Forbidden", true, &serverConfig);
		return false;
	}

	int in_pipe[2];
	int out_pipe[2];

	if (pipe(in_pipe) < 0)
	{
		perror("pipe in_pipe failed");
		conn->setShouldClose(true);
		conn->getWriteBuffer() = buildErrorResponse(500, "Internal Server Error", true, &serverConfig);
		return false;
	}

	if (pipe(out_pipe) < 0)
	{
		perror("pipe out_pip failed");
		close(in_pipe[0]);
		close(in_pipe[1]);
		conn->setShouldClose(true);
		conn->getWriteBuffer() = buildErrorResponse(500, "Internal Server Error", true, &serverConfig);
		return false;
	}
	pid_t pid = fork();
	if (pid < 0)
	{
		perror("fork failed");
		close(in_pipe[0]);
		close(in_pipe[1]);
		close(out_pipe[0]);
		close(out_pipe[1]);
		conn->setShouldClose(true);
		conn->getWriteBuffer() = buildErrorResponse(500, "Internal Server Error", true, &serverConfig);
		return false;
	}
	if (pid == 0)
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

		std::vector<std::string> envStrings;
		envStrings.push_back("REQUEST_METHOD=" + request.getMethod());
		envStrings.push_back("QUERY_STRING=" + request.getQuery());
		envStrings.push_back("SCRIPT_NAME=" + request.getPath());
		envStrings.push_back("PATH_INFO=" + request.getPath());
		envStrings.push_back("SERVER_PROTOCOL=" + request.getVersion());
		envStrings.push_back("GATEWAY_INTERFACE=CGI/1.1");

		std::ostringstream contentLength;
		contentLength << request.getBody().size();
		envStrings.push_back("CONTENT_LENGTH=" + contentLength.str());

		const std::string* contentTypeHeader = request.getHeader("content-type");
		if (contentTypeHeader != NULL && !contentTypeHeader->empty())
			envStrings.push_back("CONTENT_TYPE=" + *contentTypeHeader);
		else if (request.getMethod() == "POST")
			envStrings.push_back("CONTENT_TYPE=application/octet-stream");

		const std::string* hostHeader = request.getHeader("host");
		if (hostHeader != NULL)
			envStrings.push_back("HTTP_HOST=" + *hostHeader);

		std::vector<char*> envp;
		for (size_t i = 0; i < envStrings.size(); ++i)
			envp.push_back(const_cast<char*>(envStrings[i].c_str()));
		envp.push_back(NULL);

		char* argv[] = { const_cast<char*>(scriptPath.c_str()), NULL};

		execve(scriptPath.c_str(), argv, &envp[0]);
		perror("execve failed");
		_exit(1);
	}

	close(in_pipe[0]);
	close(out_pipe[1]);

	Connection::CGIContext* cgi = new Connection::CGIContext();
	cgi->pid = pid;
	cgi->stdin_fd = in_pipe[1];
	cgi->stdout_fd = out_pipe[0];
	cgi->stdin_closed = false;
	cgi->stdin_buffer = request.getBody();
	cgi->stdin_offset = 0;
	cgi->stdout_buffer.clear();
	cgi->start_time = std::time(NULL);

	int stdinFlags = fcntl(cgi->stdin_fd, F_GETFL, 0);
	if (stdinFlags != -1)
		fcntl(cgi->stdin_fd, F_SETFL, stdinFlags | O_NONBLOCK);
	int stdoutFlags = fcntl(cgi->stdout_fd, F_GETFL, 0);
	if (stdoutFlags != -1)
		fcntl(cgi->stdout_fd, F_SETFL, stdoutFlags | O_NONBLOCK);

	if (cgi->stdin_buffer.empty())
	{
		close(cgi->stdin_fd);
		cgi->stdin_fd = -1;
		cgi->stdin_closed = true;
	}

	conn->setCGI(cgi);
	conn->setShouldClose(shouldClose);
	conn->setState(Connection::CGI_RUNNING);
	conn->updateActivity();
	return true;
}
