/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   dummyConfig.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: Ho Wai Keong <hwai_keo@student.42kl.edu    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/18 09:35:36 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/02/18 09:56:30 by Ho Wai Keon      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Webserv.hpp"

Config::Config()
{
	initDummy();
}

const std::vector<ServerConfig>& Config::getServers()const
{
	return _servers;
}

void Config::initDummy()
{
	// first server
	ServerConfig server1;
	server1.setHost("127.0.0.1");
	server1.setPort(8080);
	server1.setServerName("test1");
	server1.setRoot("./www1");
	server1.setIndex("index.html");

	LocationConfig loc1;
	loc1.setPath("/");
	loc1.setAllowedMethods(std::vector<std::string>{"GET", "POST"});
	loc1.setUploadEnabled(false);
	
	LocationConfig loc2;
	loc2.setPath("/Upload");
	loc2.setAllowedMethods(std::vector<std::string>{"POST"});
	loc2.setUploadEnabled(true);
	loc2.setUploadPath("./uploads");

	server1.addLocation(loc1);
	server1.addLocation(loc2);
	_servers.push_back(server1);

	//second server
	ServerConfig server2;
	server2.setHost("127.0.0.1");
	server2.setPort(8081);
	server2.setServerName("test2");
	server2.setRoot("./www2");
	server2.setIndex("home.html");

	LocationConfig loc3;
	loc3.setPath("/");
	loc3.setAllowedMethods(std::vector<std::string>{"GET"});
	loc3.setUploadEnabled(false);

	server2.addLocation(loc3);
	_servers.push_back(server2);
}