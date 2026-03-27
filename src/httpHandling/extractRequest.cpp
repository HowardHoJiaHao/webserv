/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   extractRequest.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ho <hwai-keo@student.42kl.edu.my>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/27 00:59:08 by ho                #+#    #+#             */
/*   Updated: 2026/03/27 23:45:44 by ho               ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "extractRequest.hpp"
#include <sstream>
#include <cstdlib>
#include <cctype>
#include <cerrno>
#include <limits>

static std::string toLowerAscii(const std::string& input)
{
	std::string lowered = input;
	for (size_t i = 0; i < lowered.size(); ++i)
		lowered[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lowered[i])));
	return lowered;
}

static bool hasChunkedEncoding(const std::string& headers)
{
	std::istringstream stream(headers);
	std::string line;

	while (std::getline(stream, line))
	{
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);

		size_t colon = line.find(':');
		if (colon == std::string::npos)
			continue;

		std::string key = toLowerAscii(line.substr(0, colon));
		std::string value = toLowerAscii(line.substr(colon + 1));
		while (!value.empty() && (value[0] == ' ' || value[0] == '\t'))
			value.erase(0, 1);

		if (key == "transfer-encoding" && value.find("chunked") != std::string::npos)
			return true;
	}
	return false;
}

static bool parseChunkedBody(const std::string& buffer, size_t bodyStart, size_t &totalSize)
{
	size_t pos = bodyStart;

	while (true)
	{
		size_t lineEnd = buffer.find("\r\n", pos);
		if (lineEnd == std::string::npos)
			return false;

		std::string hexSize = buffer.substr(pos, lineEnd - pos);
		char* endptr = NULL;
		size_t chunkSize = std::strtoul(hexSize.c_str(), &endptr, 16);
		if (endptr == hexSize.c_str() || *endptr != '\0')
			return false;

		pos = lineEnd + 2;

		if (buffer.size() < pos + chunkSize + 2)
			return false;

		pos += chunkSize;

		if (buffer.substr(pos, 2) != "\r\n")
			return false;

		pos += 2;
		if (chunkSize == 0)
		{
			totalSize = pos;
			return true;
		}
	}
}

bool extractRequest(std::string& buffer, std::string& rawRequest)
{
	size_t	headerEnd = buffer.find("\r\n\r\n");
	if (headerEnd == std::string::npos)
		return false;

	size_t bodyStart = headerEnd + 4;

	std::string headersPart = buffer.substr(0, headerEnd);
	if(hasChunkedEncoding(headersPart))
	{
		size_t totalSize = 0;

		if (!parseChunkedBody(buffer, bodyStart, totalSize))
			return false;
		rawRequest = buffer.substr(0, totalSize);
		buffer.erase(0, totalSize);
		return true;
	}

	size_t contentLength = 0;
	bool hasContentLength = false;

	std::istringstream stream(headersPart);
	std::string line;

	while (std::getline(stream, line))
	{
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);

		size_t colon = line.find(':');
		if (colon == std::string::npos)
			continue;

		std::string key = toLowerAscii(line.substr(0, colon));
		std::string value = line.substr(colon + 1);

		while (!value.empty() && (value[0] == ' ' || value[0] == '\t'))
			value.erase(0, 1);
		if (key == "content-length")
		{
			if (value.empty())
				return false;

			errno = 0;
			char* endptr = NULL;
			unsigned long parsed = std::strtoul(value.c_str(), &endptr, 10);
			if (endptr == value.c_str() || *endptr != '\0' || errno == ERANGE)
				return false;
			if (parsed > std::numeric_limits<size_t>::max())
				return false;

			contentLength = static_cast<size_t>(parsed);
			hasContentLength = true;
		}
	}
	size_t totalSize = bodyStart;
	if (hasContentLength)
		totalSize += contentLength;
	if (buffer.size() < totalSize)
		return false;

	rawRequest = buffer.substr(0, totalSize);
	buffer.erase(0, totalSize);

	return true;
}