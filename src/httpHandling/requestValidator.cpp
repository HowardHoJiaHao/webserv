/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   requestValidator.cpp                               :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hwai-keo <hwai_keo@student.42kl.edu.my>    +#+  +#+#+#+#+#+   +#+           */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/27 00:59:08 by ho                #+#    #+#             */
/*   Updated: 2026/04/07 17:30:00 by hwai-keo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "requestValidator.hpp"
#include <sstream>
#include <cstdlib>
#include <cctype>
#include <cerrno>

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

static bool validateAndMeasureChunkedBody(const std::string& buffer, size_t bodyStart, size_t& totalSize)
{
	size_t pos = bodyStart;

	while (true)
	{
		size_t lineEndPos = buffer.find("\r\n", pos);
		if (lineEndPos == std::string::npos)
			return false;

		std::string hexSize = buffer.substr(pos, lineEndPos - pos);
		while (!hexSize.empty() && (hexSize[hexSize.size() - 1] == ' ' || hexSize[hexSize.size() - 1] == '\t'))
			hexSize.erase(hexSize.size() - 1);

		char* endptr = NULL;
		size_t byteToRead = std::strtoul(hexSize.c_str(), &endptr, 16);
		if (endptr == hexSize.c_str() || *endptr != '\0')
			return false;

		pos = lineEndPos + 2;
		if (buffer.size() < pos + byteToRead + 2)
			return false;
		pos += byteToRead;
		if (buffer.substr(pos, 2) != "\r\n")
			return false;
		pos += 2;
		if (byteToRead == 0)
		{
			totalSize = pos;
			return true;
		}
	}
}

static bool parseContentLength(const std::string& headersPart, size_t& contentLength, bool& hasContentLength, bool* isInvalidContentLength)
{
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
			if (hasContentLength)
			{
				if (isInvalidContentLength != NULL)
					*isInvalidContentLength = true;
				return false;
			}
			if (value.empty())
			{
				if (isInvalidContentLength != NULL)
					*isInvalidContentLength = true;
				return false;
			}

			errno = 0;
			char* endptr = NULL;
			unsigned long parsed = std::strtoul(value.c_str(), &endptr, 10);
			if (endptr == value.c_str() || *endptr != '\0' || errno == ERANGE)
			{
				if (isInvalidContentLength != NULL)
					*isInvalidContentLength = true;
				return false;
			}
			contentLength = static_cast<size_t>(parsed);
			hasContentLength = true;
		}
	}

	return true;
}

bool validateAndExtractRequestFromBuffer(std::string& buffer, std::string& rawRequest, bool* isInvalidContentLength)
{
	if (isInvalidContentLength != NULL)
		*isInvalidContentLength = false;

	size_t headerEnd = buffer.find("\r\n\r\n");
	if (headerEnd == std::string::npos)
		return false;
	size_t bodyStart = headerEnd + 4;
	std::string headersPart = buffer.substr(0, headerEnd);

	if (hasChunkedEncoding(headersPart))
	{
		size_t requestEndPos = 0;
		if (!validateAndMeasureChunkedBody(buffer, bodyStart, requestEndPos))
			return false;
		rawRequest = buffer.substr(0, requestEndPos);
		buffer.erase(0, requestEndPos);
		return true;
	}

	size_t contentLength = 0;
	bool hasContentLength = false;
	if (!parseContentLength(headersPart, contentLength, hasContentLength, isInvalidContentLength))
		return false;

	size_t totalSize = bodyStart;
	if (hasContentLength)
		totalSize += contentLength;

	if (buffer.size() < totalSize)
		return false;

	rawRequest = buffer.substr(0, totalSize);
	buffer.erase(0, totalSize);
	return true;
}