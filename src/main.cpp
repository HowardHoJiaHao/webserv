/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ho <hwai-keo@student.42kl.edu.my>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/17 22:23:54 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/03/28 01:00:13 by ho               ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Webserv.hpp"
#include "ConfigFiles.hpp"
#include "engine.hpp"

void	printServerStatus(const std::vector<ServerConfig>& servers)
{
	for (size_t i = 0; i < servers.size(); i++)
	{
		std::cout << "Server " << i << std::endl;
		std::cout << "Port: " << servers[i].getPort() << std::endl;
		std::cout << "Host: " << servers[i].getHost() << std::endl;
	}
}

int	main(int argc, char **argv)
{
	if (argc == 1 || argc == 2)
	{
		try
		{
			ConfigFiles		config;
			if (argc == 2)
			{
				std::cout << "compile with config" << std::endl;
				config = ConfigFiles(argv[1]);
			}
			Engine	engine(config);
			engine.setupListeningSockets();
			engine.run();
			// const std::vector<ServerConfig>& servers = config.getServers();
			// printServerStatus(servers); //debug testing
			
			
		}
		catch (std::exception &e)
		{
			std::cerr << e.what() << std::endl;
			return (1);
		}
	}
	else
	{
		std::cerr << "problem in argument " << std::endl;
		return (1);
	}
	return (0);
}

// the config is comes from argv