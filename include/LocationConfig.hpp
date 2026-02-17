/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   LocationConfig.hpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: Ho Wai Keong <hwai_keo@student.42kl.edu    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/17 23:29:41 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/02/17 23:35:52 by Ho Wai Keon      ###   ########.fr       */
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
		bool						_uploadEnabled;
		std::string					_uploadPath;

	public:
		LocationConfig();

		void	setPath(const std::string& path);
		void	setAllowedMethods(const std::vector<std::string>& method);
		void	setRoot(const std::string& root);
		void	setUploadENabled(bool enabled);
		void	setUploadPath(const std::string& path);

		const	std::string& getPath() const;
		const	std::vector<std::string>& getAllowedMethods() const;
		const	std::string& getRoot() const;
		bool	isUploadEnabled() const;
		const	std::string& getUploadPath() const;
};

#endif