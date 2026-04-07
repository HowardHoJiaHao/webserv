/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   extractRequest.hpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hwai-keo <hwai-keo@student.42kl.edu.my>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/27 00:57:35 by ho                #+#    #+#             */
/*   Updated: 2026/04/06 14:25:56 by hwai-keo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef EXTRACT_REQUEST_HPP
#define EXTRACT_REQUEST_HPP

#include <string>

bool	extractRequest(std::string& buffer, std::string& rawRequest, bool* isInvalidContentLength);

#endif