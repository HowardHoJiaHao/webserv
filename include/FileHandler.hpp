/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   FileHandler.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ho <hwai-keo@student.42kl.edu.my>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/23 01:50:06 by ho                #+#    #+#             */
/*   Updated: 2026/03/23 02:43:57 by ho               ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef FILEHANDLER_HPP
#define FILEHANDLER_HPP

#include <string>
#include "httpRequest.hpp"

class FileHandler
{
	public:
		static std::string	resolvePath(const std::string& urlPath);
		static bool			fileExists(const std::string& path);
		static				std::string readFile(const std::string& path);
		static				std::string getMimeType(const std::string& path);
};


#endif