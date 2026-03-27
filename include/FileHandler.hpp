/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   FileHandler.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ho <hwai-keo@student.42kl.edu.my>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/23 01:50:06 by ho                #+#    #+#             */
/*   Updated: 2026/03/28 00:34:18 by ho               ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef FILEHANDLER_HPP
#define FILEHANDLER_HPP

#include <string>
#include "httpRequest.hpp"

class FileHandler
{
	public:
		static std::string	resolvePath(const std::string& urlPath, const std::string& root, const std::string& index);
		static bool			fileExists(const std::string& path);
		static				std::string readFile(const std::string& path);
		static				std::string getMimeType(const std::string& path);
		static std::string	generateDirectoryListing(const std::string& urlPath, const std::string& fsPath);
};


#endif