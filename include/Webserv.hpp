/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Webserv.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hwai-keo <hwai-keo@student.42kl.edu.my>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/17 22:29:16 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/04/03 17:25:19 by hwai-keo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef WEBSERV_HPP
#define WEBSERV_HPP

#define MAX_REQUEST_SIZE 1000000
#define MAX_HEADER_SIZE 8192

#include <iostream>
#include <stdexcept>
#include "ConfigFiles.hpp"
#include "engine.hpp"
#include "engine_string_utils.hpp"
#include "socket_utils.hpp"
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <stdexcept>
#include <signal.h>
#include <sys/select.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sstream>
#include <cstdio>
#include <cerrno>
#include <ctime>
#include <sys/wait.h>
#include <csignal>


#endif