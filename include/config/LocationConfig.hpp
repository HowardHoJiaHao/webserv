/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Location.hpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ktiew <ktiew@student.42kl.edu.my>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/20 12:29:07 by ktiew             #+#    #+#             */
/*   Updated: 2026/02/21 23:54:27 by ktiew            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef LOCATIONCONFIG_HPP
#define LOCATIONCONFIG_HPP

#include <string>
#include <vector>

class LocationConfig
{
	private:
		std::string					_path;
		std::vector<std::string>	_allowedMethods;
		std::string					_root;
		std::string					_index;
		bool						_uploadEnabled;
		std::string					_uploadPath;
		bool						_autoindex;
		bool						_cgiEnabled;
		std::vector<std::string>	_cgiExtensions;
		bool						_hasReturn;
		int							_returnStatus;
		std::string					_returnTarget;

	public:
		LocationConfig(void);
		~LocationConfig(void);

		void	setPath(const std::string& path);
		void	setAllowedMethods(const std::vector<std::string>& method);
		void	setRoot(const std::string& root);
		void	setIndex(const std::string& index);
		void	setUploadEnabled(bool enabled);
		void	setUploadPath(const std::string& path);
		void	setAutoindex(bool enabled);
		void	setCgiEnabled(bool enabled);
		void	setCgiExtensions(const std::vector<std::string>& extensions);
		void	setReturnDirective(bool enabled, int status, const std::string& target);

		const std::string&				getPath(void) const;
		const std::vector<std::string>&	getAllowedMethods(void) const;
		const std::string&				getRoot(void) const;
		const std::string&				getIndex(void) const;
		bool							isUploadEnabled(void) const;
		const std::string&				getUploadPath(void) const;
		bool							isAutoindex(void) const;
		bool							isCgiEnabled(void) const;
		const std::vector<std::string>&	getCgiExtensions(void) const;
		bool							hasReturnDirective(void) const;
		int								getReturnStatus(void) const;
		const std::string&				getReturnTarget(void) const;
};

#endif
