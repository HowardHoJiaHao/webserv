/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   LocationConfig.hpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hwai-keo <hwai-keo@student.42kl.edu.my>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/17 23:29:41 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/04/08 17:29:26 by hwai-keo         ###   ########.fr       */
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
		LocationConfig();

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

		const	std::string& getPath() const;
		const	std::vector<std::string>& getAllowedMethods() const;
		const	std::string& getRoot() const;
		const	std::string& getIndex() const;
		bool	isUploadEnabled() const;
		const	std::string& getUploadPath() const;
		bool	isAutoindex() const;
		bool	isCgiEnabled() const;
		const	std::vector<std::string>& getCgiExtensions() const;
		bool	hasReturnDirective() const;
		int		getReturnStatus() const;
		const	std::string& getReturnTarget() const;
};

#endif