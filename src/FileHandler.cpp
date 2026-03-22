/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   FileHandler.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ho <hwai-keo@student.42kl.edu.my>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/23 01:54:02 by ho                #+#    #+#             */
/*   Updated: 2026/03/23 02:47:20 by ho               ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "FileHandler.hpp"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

std::string FileHandler::resolvePath(const std::string& urlPath)
{
	if (urlPath == "/")
		return "./www/index.html";
	return "./www" + urlPath;
}

bool FileHandler::fileExists(const std::string& path)
{
	struct stat buffer;
	return (stat(path.c_str(), &buffer) == 0);
}

std::string FileHandler::readFile(const std::string& path)
{
	int fd = open(path.c_str(), O_RDONLY);
	if (fd < 0)
		return "";
	char buffer[1024];
	std::string content;
	ssize_t bytesRead;

	while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0)
	{
		content.append(buffer, bytesRead);
	}
	close(fd);
	return content;
}

std::string FileHandler::getMimeType(const std::string& path)
{
	if (path.size() >= 5 && path.substr(path.size() - 5) == ".html")
		return "text/html";
	if (path.size() >= 4 && path.substr(path.size() - 4) == ".txt")
		return "text/plain";
	return "application/octet-stream";
}