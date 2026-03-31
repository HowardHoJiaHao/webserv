/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   socket_utils.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hwai-keo <hwai-keo@student.42kl.edu.my>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/24 17:20:00 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/03/31 16:01:22 by hwai-keo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SOCKET_UTILS_HPP
#define SOCKET_UTILS_HPP

#include <string>
#include <netdb.h>

// Setup address info query for socket resolution
struct addrinfo* setupAddrInfoQuery(const std::string& host, int port);

// Attempt to create and bind a socket from addrinfo results
int attemptBindingSocket(struct addrinfo* osRes, int& lastErrno);

// Configure socket for listening and non-blocking mode
void configureListeningNonblockingSocket(int& sockfd);

// Create a complete listening socket (combines all steps above)
int createListeningSocket(const std::string& host, int port);

#endif
