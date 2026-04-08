/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   requestValidator.hpp                               :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hwai-keo <hwai_keo@student.42kl.edu.my>    +#+  +#+#+#+#+#+   +#+           */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/07 17:30:00 by hwai-keo          #+#    #+#             */
/*   Updated: 2026/04/07 17:30:00 by hwai-keo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef REQUESTVALIDATOR_HPP
#define REQUESTVALIDATOR_HPP

#include <string>

bool hasChunkedTransferEncodingValue(const std::string& value);
bool decodeChunkedBodyForRequest(const std::string& encodedBody, size_t maxBodySize, std::string& decodedBody, bool* bodyTooLarge);
bool validateAndExtractRequestFromBuffer(std::string& buffer, std::string& rawRequest, bool* isInvalidContentLength);

#endif