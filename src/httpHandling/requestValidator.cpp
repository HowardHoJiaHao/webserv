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
#include "engine_string_utils.hpp"
#include <sstream>
#include <cstdlib>
#include <cerrno>

// possible for multiple encoding
// Transfer-Encoding: gzip, chunked <= header
bool hasChunkedTransferEncodingValue(const std::string& value)
{
	std::istringstream iss(toLowerAsciiEngine(value));
	std::string token;
	// handle multiple encoding
	// third parameter means split input using ',' as seperator
	// read until comma, give me that piece
	while (std::getline(iss, token, ','))
	{
		if (trimAsciiEngine(token) == "chunked")
			return true;
	}
	return false;
}

// read line by line, check is this transfer-encoding exist
// example of header
// POST /upload HTTP/1.1
// Host: example.com
// Transfer-Encoding: chunked

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

		std::string key = toLowerAsciiEngine(line.substr(0, colon));
		std::string value = trimAsciiEngine(line.substr(colon + 1));

		if (key == "transfer-encoding" && hasChunkedTransferEncodingValue(value))
			return true;
	}
	return false;
}

// line is: hex number (\r\n has removed)
// chunkSize is output: number for the caller to use later
static bool parseChunkSizeLine(const std::string& line, size_t& chunkSize)
{
	std::string sizeToken = line;
	// ex: 4;abc=4 -> 4
	size_t extensionPos = sizeToken.find(';');
	// remove extension, no real use case
	if (extensionPos != std::string::npos)
		sizeToken = sizeToken.substr(0, extensionPos);
	// trim space
	sizeToken = trimAsciiEngine(sizeToken);
	if (sizeToken.empty())
		return false;

	// convert hex to number
	errno = 0;
	char* endptr = NULL;
	unsigned long parsed = std::strtoul(sizeToken.c_str(), &endptr, 16);
	if (endptr == sizeToken.c_str() || *endptr != '\0' || errno == ERANGE)
		return false;

	// output and cast it
	chunkSize = static_cast<size_t>(parsed);
	return true;
}

static bool parseChunkedBodyInternal(const std::string& buffer, size_t bodyStart, size_t maxBodySize, size_t& totalSize, std::string* decodedBody, bool* bodyTooLarge)
{
	size_t pos = bodyStart;
	if (bodyTooLarge != NULL)
		*bodyTooLarge = false;
	if (decodedBody != NULL)
		decodedBody->clear();

	while (true)
	{
		size_t lineEndPos = buffer.find("\r\n", pos);
		if (lineEndPos == std::string::npos)
			return false;

		size_t byteToRead = 0;
		if (!parseChunkSizeLine(buffer.substr(pos, lineEndPos - pos), byteToRead))
			return false;

		pos = lineEndPos + 2;
		if (buffer.size() < pos + byteToRead + 2)
			return false;

		if (decodedBody != NULL && byteToRead > 0)
		{
			if (decodedBody->size() + byteToRead > maxBodySize)
			{
				if (bodyTooLarge != NULL)
					*bodyTooLarge = true;
				return false;
			}
			decodedBody->append(buffer, pos, byteToRead);
		}

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

// buffer
// POST /upload HTTP/1.1\r\n
// Host: example.com\r\n
// Transfer-Encoding: chunked\r\n
// \r\n
// 4\r\n	<=== body start here
// Wiki\r\n
// 5\r\n
// pedia\r\n
// 0\r\n
// \r\n

// this is about to validate if the pattern is correct
// any false leads to return and wait new data receive, append new data to existing buffer
static bool validateAndMeasureChunkedBody(const std::string& buffer, size_t bodyStart, size_t& totalSize)
{
	size_t pos = bodyStart;

	while (true)
	{
		// will point to \r after the number(chunkedSize)
		size_t lineEndPos = buffer.find("\r\n", pos);
		if (lineEndPos == std::string::npos)
			return false;

		// fetch the number(chunkSize)
		size_t byteToRead = 0;
		if (!parseChunkSizeLine(buffer.substr(pos, lineEndPos - pos), byteToRead))
			return false;

		// pos : move beyond \r\n, pos points to next character after \n
		pos = lineEndPos + 2;
		// buffer.size(): what i received so far
		// pos + byteToRead + 2: what is safely to read next chunk
		if (buffer.size() < pos + byteToRead + 2)
			return false;

		// pos: move after the string, expecting it will be \r\n
		pos += byteToRead;
		if (buffer.substr(pos, 2) != "\r\n")
			return false;
		// skip \r\n
		pos += 2;

		// end of chunk encoding
		if (byteToRead == 0)
		{
			totalSize = pos;
			return true;
		}
	}
}
// for parsing and decoding chunked body
bool decodeChunkedBodyForRequest(const std::string& encodedBody, size_t maxBodySize, std::string& decodedBody, bool* bodyTooLarge)
{
	size_t totalSize = 0;
	if (!parseChunkedBodyInternal(encodedBody, 0, maxBodySize, totalSize, &decodedBody, bodyTooLarge))
		return false;
	return totalSize == encodedBody.size();
}

static bool parseContentLength(const std::string& headersPart, size_t& contentLength, bool& hasContentLength, bool* isInvalidContentLength)
{
	std::istringstream stream(headersPart);
	std::string line;

	//iterate over header lines
	while (std::getline(stream, line))
	{
		// remove trailing \r so that can search the key
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);

		// find :
		size_t colon = line.find(':');
		if (colon == std::string::npos)
			continue;

		// split key value and normalize it
		std::string key = toLowerAsciiEngine(line.substr(0, colon));
		std::string value = trimAsciiEngine(line.substr(colon + 1));

		// detect keyword
		if (key == "content-length")
		{
			// reject duplicate
			if (hasContentLength)
			{
				if (isInvalidContentLength != NULL)
					*isInvalidContentLength = true;
				return false;
			}
			// no value, reject
			if (value.empty())
			{
				if (isInvalidContentLength != NULL)
					*isInvalidContentLength = true;
				return false;
			}

			// parse it from str to ul in base 10
			errno = 0;
			char* endptr = NULL;
			unsigned long parsed = std::strtoul(value.c_str(), &endptr, 10);
			// endptr points to any character? not at null terminator? overflow?
			if (endptr == value.c_str() || *endptr != '\0' || errno == ERANGE)
			{
				// set it invalid and return false
				if (isInvalidContentLength != NULL)
					*isInvalidContentLength = true;
				return false;
			}
			// store it the value and return the value to caller
			contentLength = static_cast<size_t>(parsed);
			hasContentLength = true;
		}
	}
	// only invalid content-lengh return false, no content length still return true
	return true;
}

// i will take buffer content, cut and paste to rawRequest, and erase the buffer
// true means the request is complete, and the rawRequest is filled
bool httpRequestCompletenessChecking(std::string& buffer, std::string& rawRequest, bool* isInvalidContentLength)
{
	if (isInvalidContentLength != NULL)
		*isInvalidContentLength = false;

	// evaluate bodyStart pos, and extract headerPart
	size_t headerEnd = buffer.find("\r\n\r\n");
	if (headerEnd == std::string::npos)
		return false;
	size_t bodyStart = headerEnd + 4;
	std::string headersPart = buffer.substr(0, headerEnd);

	// is header using chunkEncoding
	if (hasChunkedEncoding(headersPart))
	{
		//validate chunked structure, check is body is complete, check completeness
		size_t requestEndPos = 0;
		if (!validateAndMeasureChunkedBody(buffer, bodyStart, requestEndPos))
			return false;
		// only extract to rawRequest when request is full
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

	// if buffer is not enough, return false
	// expect to wait more bytes from recv()
	if (buffer.size() < totalSize)
		return false;

	// only extract to rawRequest when request is full (complete)
	rawRequest = buffer.substr(0, totalSize);
	buffer.erase(0, totalSize);
	return true;
}