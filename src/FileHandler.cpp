/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   FileHandler.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hwai-keo <hwai-keo@student.42kl.edu.my>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/23 01:54:02 by ho                #+#    #+#             */
/*   Updated: 2026/04/10 18:55:12 by hwai-keo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "FileHandler.hpp"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

// building the final file path
std::string FileHandler::resolvePath(const std::string& urlPathFromRequest, const std::string& root, const std::string& index)
{
	// use index only when urlPathFromRequest is "/"
	if (urlPathFromRequest == "/")
		return root + "/" + index;
	// otherwise, ignore index
	return root + urlPathFromRequest;
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
	if (path.size() >= 4 && path.substr(path.size() - 4) == ".css")
		return "text/css";
	if (path.size() >= 3 && path.substr(path.size() - 3) == ".js")
		return "application/javascript";
	if (path.size() >= 4 && path.substr(path.size() - 4) == ".png")
		return "image/png";
	if (path.size() >= 4 && path.substr(path.size() - 4) == ".jpg")
		return "image/jpeg";
	if (path.size() >= 5 && path.substr(path.size() - 5) == ".jpeg")
		return "image/jpeg";
	if (path.size() >= 4 && path.substr(path.size() - 4) == ".gif")
		return "image/gif";
	if (path.size() >= 5 && path.substr(path.size() - 5) == ".json")
		return "application/json";
	return "application/octet-stream";
}

std::string FileHandler::generateDirectoryListing(const std::string& urlPath, const std::string& fsPath)
{
	DIR* dir = opendir(fsPath.c_str());
	if (!dir)
		return "";

	std::string body = "<html><head><title>Index of " + urlPath + "</title></head><body>";
	body += "<h1>Index of " + urlPath + "</h1><hr><pre>";

	struct dirent* entry;
	while ((entry = readdir(dir)) != NULL)
	{
		std::string name = entry->d_name;
		if (name == ".")
			continue;
		body += "<a href=\"" + urlPath;
		if (!urlPath.empty() && urlPath[urlPath.size() - 1] != '/')
			body += "/";
		body += name + "\">" + name + "</a>\n";
	}
	closedir(dir);
	body += "</pre><hr></body></html>";
	return body;
}