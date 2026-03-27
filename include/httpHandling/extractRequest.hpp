/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   extractRequest.hpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ho <hwai-keo@student.42kl.edu.my>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/27 00:57:35 by ho                #+#    #+#             */
/*   Updated: 2026/03/28 00:34:22 by ho               ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef EXTRACT_REQUEST_HPP
#define EXTRACT_REQUEST_HPP

#include <string>

bool	extractRequest(std::string& buffer, std::string& rawRequest, bool* malformed);

#endif