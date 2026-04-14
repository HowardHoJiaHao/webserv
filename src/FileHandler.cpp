/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   FileHandler.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ho <hwai-keo@student.42kl.edu.my>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/23 01:54:02 by ho                #+#    #+#             */
/*   Updated: 2026/04/11 02:43:01 by ho               ###   ########.fr       */
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

// 1. path = "./www1/index.html"
// 2. the content of the file content is <html><body>Hello</body></html>
// 3. return content = "<html><body>Hello</body></html>"
std::string FileHandler::readFile(const std::string& path)
{
	// open file
	int fd = open(path.c_str(), O_RDONLY);
	if (fd < 0)
		return "";
	// prepare buffer
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

// path example
// "./www1/index.html"
// "./www1/style.css"
// "./www1/script.js"
// "./www1/image.png"
// "./www1/blog/post.html"
//
// substr take last 5 chars and compare
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

// generate html page listing files in directory
//
// .
// ..
// index.html
// style.css
// images

std::string FileHandler::generateDirectoryListing(const std::string& urlPath, const std::string& fsPath)
{
	//try to oen directory on disk
	DIR* dir = opendir(fsPath.c_str());
	if (!dir)
		return "";

	// build html page with title
	std::string body = "<html><head><title>Index of " + urlPath + "</title></head><body>";
	// add header and prepare formatted listing session
	body += "<h1>Index of " + urlPath + "</h1><hr><pre>";

	// entry is a variable to hold each file entry
	struct dirent* entry;
	// loop through each file in directory
	while ((entry = readdir(dir)) != NULL)
	{
		//extract file name
		std::string name = entry->d_name;
		// skip current directory. not ..(previous directory)
		if (name == ".")
			continue;
		// building clickable link
		body += "<a href=\"" + urlPath;
		// ensure URL ends with / before appending file name
		if (!urlPath.empty() && urlPath[urlPath.size() - 1] != '/')
			body += "/";
		// add link text and close html tag
		body += name + "\">" + name + "</a>\n";
	}
	//close directory to free resources
	closedir(dir);
	// finish html page
	body += "</pre><hr></body></html>";
	return body;
}