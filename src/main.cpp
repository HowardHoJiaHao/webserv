/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hwai-keo <hwai-keo@student.42kl.edu.my>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/17 22:23:54 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/04/10 13:06:21 by hwai-keo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Webserv.hpp"
#include "ConfigFiles.hpp"
#include "engine.hpp"
#include <csignal>
#include <ctime>
#include <cstdlib>

int	main(int argc, char **argv)
{
	//when a client disconnects, SIGPIPE is sent, but we ignore it to avoid crashing
	// the moment the server tries to write to the client, the client disconnect at the same time
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
