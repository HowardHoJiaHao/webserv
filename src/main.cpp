/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hwai-keo <hwai-keo@student.42kl.edu.my>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/17 22:23:54 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/03/31 16:48:21 by hwai-keo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Webserv.hpp"
#include "ConfigFiles.hpp"
#include "engine.hpp"
#include <csignal>
#include <ctime>
#include <cstdlib>

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
	signal(SIGPIPE, SIG_IGN);
	std::srand(static_cast<unsigned int>(std::time(NULL)));

	if (argc == 1 || argc == 2)
	{
		try
		{
			ConfigFiles		config(argc == 2 ? argv[1] : "");
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