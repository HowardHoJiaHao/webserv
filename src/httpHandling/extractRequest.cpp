/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   extractRequest.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hwai-keo <hwai-keo@student.42kl.edu.my>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/27 00:59:08 by ho                #+#    #+#             */
/*   Updated: 2026/04/06 19:05:24 by hwai-keo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "extractRequest.hpp"
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

// example of chunkedEncoding
// POST /upload HTTP/1.1\r\n
// Host: example.com\r\n
// Transfer-Encoding: chunked\r\n
// \r\n
// 5\r\n
// Hello\r\n
// 6\r\n
//  World\r\n
// 0\r\n
// \r\n
static bool hasChunkedEncoding(const std::string& headers)
{
	// enable line by line reading
	std::istringstream stream(headers);
	std::string line;

	// get line will automatically remove \n, not \r
	while (std::getline(stream, line))
	{
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);

		size_t colon = line.find(':');
		if (colon == std::string::npos)
			continue;

		//normalize every charactor
		std::string key = toLowerAscii(line.substr(0, colon));
		std::string value = toLowerAscii(line.substr(colon + 1));

		// trim leading space
		while (!value.empty() && (value[0] == ' ' || value[0] == '\t'))
			value.erase(0, 1);

		// the valiue that we want to find (without \r)
		if (key == "transfer-encoding" && value.find("chunked") != std::string::npos)
			return true;
	}
	return false;
}

// the buffer is sth like this
// 5\r\n		<- this is hex not decimal
// Hello\r\n
// 0\r\n\r\n
//
static bool validateAndMeasureChunkedBody(const std::string& buffer, size_t bodyStart, size_t &totalSize)
{
	size_t pos = bodyStart;

	while (true)
	{
		size_t lineEndPos = buffer.find("\r\n", pos);
		if (lineEndPos == std::string::npos)
			return false;

		std::string hexSize = buffer.substr(pos, lineEndPos - pos);
		// remove trailing spaces and tabs from the end of hexsize
		while (!hexSize.empty() && (hexSize[hexSize.size() - 1] == ' ' || hexSize[hexSize.size() - 1] == '\t'))
			hexSize.erase(hexSize.size() - 1);

		char* endptr = NULL;
		// it doesnt really handle the ';', eg: 5;abc=123
		// strtoul ignore the leading spaces
		size_t byteToRead = std::strtoul(hexSize.c_str(), &endptr, 16);
		if (endptr == hexSize.c_str() || *endptr != '\0') //expecting endptr pointing to null terminator
			return false;

		pos = lineEndPos + 2;
		if (buffer.size() < pos + byteToRead + 2)
			return false;
		pos += byteToRead;
		if (buffer.substr(pos, 2) != "\r\n")
			return false;
		pos += 2;
		// 0 is for the end of the body
		if (byteToRead == 0)
		{
			totalSize = pos;
			return true;
		}
	}
}
//output to get total size and bool
// example
// Content-Length: 5\r\n
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
				// Duplicate Content-Length header — reject immediately
				if (isInvalidContentLength != NULL)
					*isInvalidContentLength = true;
				return false;
			}
			// value is empty
			if (value.empty())
			{
				if (isInvalidContentLength != NULL)
					*isInvalidContentLength = true;
				return false;
			}

			//parse Number
			errno = 0;
			char* endptr = NULL;
			unsigned long parsed = std::strtoul(value.c_str(), &endptr, 10); //convert to the base 10
			// if endptr is -> alphabet, still pointing towards sth, pointing to error(very large num)
			if (endptr == value.c_str() || *endptr != '\0' || errno == ERANGE)
			{
				if (isInvalidContentLength != NULL)
					*isInvalidContentLength = true;
				return false;
			}
			//store result
			contentLength = static_cast<size_t>(parsed);
			// it will modify the caller variable flag
			hasContentLength = true;
		}
	}


	return true;
}

// transform the buffer ->> rawRequest
//
// example of chunkedEncoding
// POST /upload HTTP/1.1\r\n
// Host: example.com\r\n
// Transfer-Encoding: chunked\r\n
// \r\n
// 5\r\n
// Hello\r\n
// 6\r\n
//  World\r\n
// 0\r\n
// \r\n
bool extractRequest(std::string& buffer, std::string& rawRequest, bool* isInvalidContentLength)
{
	//reset
	if (isInvalidContentLength != NULL)
		*isInvalidContentLength = false;

	// find headerEnd, if cannot find, then cannot extract
	size_t	headerEnd = buffer.find("\r\n\r\n");
	if (headerEnd == std::string::npos)
		return false;
	// get the position of the first body
	size_t bodyStart = headerEnd + 4;
	// isolated the header part
	std::string headersPart = buffer.substr(0, headerEnd);

	// this is for chunked header
	if(hasChunkedEncoding(headersPart))
	{
		// i want to get the totalSize
		size_t requestEndPos = 0;

		if (!validateAndMeasureChunkedBody(buffer, bodyStart, requestEndPos))
			return false;
		// if complete
		rawRequest = buffer.substr(0, requestEndPos);
		buffer.erase(0, requestEndPos);
		return true;
	}

	// parse content length
	size_t contentLength = 0;
	bool hasContentLength = false;
	// now i have content length
	if (!parseContentLength(headersPart, contentLength, hasContentLength, isInvalidContentLength))
		return false;
	//compute total size
	size_t totalSize = bodyStart;
	if (hasContentLength)
		totalSize += contentLength;
	
	if (buffer.size() < totalSize)
		return false;

	//extract
	// the rawRequest is included \r\n\r\n
	rawRequest = buffer.substr(0, totalSize);
	buffer.erase(0, totalSize);
	return true;
}
