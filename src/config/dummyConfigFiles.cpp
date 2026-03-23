/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   dummyConfigFiles.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: Ho Wai Keong <hwai_keo@student.42kl.edu    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/18 09:35:36 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/02/19 10:23:31 by Ho Wai Keon      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Webserv.hpp"

ConfigFiles::ConfigFiles()
{
	initDummy();
}

const std::vector<ServerConfig>& ConfigFiles::getServers() const
{
	return _serverConfigs;
}

void ConfigFiles::initDummy()
{
	// first server
	std::cout << "printing server" << std::endl;
	ServerConfig server1;
	server1.setHost("127.0.0.1");
	server1.setPort(8080);
	server1.setServerName("test1");
	server1.setRoot("./www1");
	server1.setIndex("index.html");

	LocationConfig loc1;
	loc1.setPath("/");
	{
		std::vector<std::string> methods;
		methods.push_back("GET");
		methods.push_back("POST");
		loc1.setAllowedMethods(methods);
	}
	loc1.setUploadEnabled(false);
	
	LocationConfig loc2;
	loc2.setPath("/Upload");
	{
		std::vector<std::string> methods;
		methods.push_back("POST");
		loc2.setAllowedMethods(methods);
	}
	loc2.setUploadEnabled(true);
	loc2.setUploadPath("./uploads");

	server1.addLocation(loc1);
	server1.addLocation(loc2);
	_serverConfigs.push_back(server1);

	//second server
	ServerConfig server2;
	server2.setHost("127.0.0.1");
	server2.setPort(8081);
	server2.setServerName("test2");
	server2.setRoot("./www2");
	server2.setIndex("home.html");

	LocationConfig loc3;
	loc3.setPath("/");
	{
		std::vector<std::string> methods;
		methods.push_back("GET");
		loc3.setAllowedMethods(methods);
	}
	loc3.setUploadEnabled(false);

	server2.addLocation(loc3);
	_serverConfigs.push_back(server2);
}