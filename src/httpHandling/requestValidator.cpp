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

bool hasChunkedTransferEncodingValue(const std::string& value)
{
	std::istringstream iss(toLowerAsciiEngine(value));
	std::string token;
	while (std::getline(iss, token, ','))
	{
		if (trimAsciiEngine(token) == "chunked")
			return true;
	}
	return false;
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

		std::string key = toLowerAsciiEngine(line.substr(0, colon));
		std::string value = trimAsciiEngine(line.substr(colon + 1));

		if (key == "transfer-encoding" && hasChunkedTransferEncodingValue(value))
			return true;
	}
	return false;
}

static bool parseChunkSizeLine(const std::string& line, size_t& chunkSize)
{
	std::string sizeToken = line;
	size_t extensionPos = sizeToken.find(';');
	if (extensionPos != std::string::npos)
		sizeToken = sizeToken.substr(0, extensionPos);
	sizeToken = trimAsciiEngine(sizeToken);
	if (sizeToken.empty())
		return false;

	errno = 0;
	char* endptr = NULL;
	unsigned long parsed = std::strtoul(sizeToken.c_str(), &endptr, 16);
	if (endptr == sizeToken.c_str() || *endptr != '\0' || errno == ERANGE)
		return false;

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

static bool validateAndMeasureChunkedBody(const std::string& buffer, size_t bodyStart, size_t& totalSize)
{
	return parseChunkedBodyInternal(buffer, bodyStart, 0, totalSize, NULL, NULL);
}

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

	while (std::getline(stream, line))
	{
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);

		size_t colon = line.find(':');
		if (colon == std::string::npos)
			continue;

		std::string key = toLowerAsciiEngine(line.substr(0, colon));
		std::string value = trimAsciiEngine(line.substr(colon + 1));

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