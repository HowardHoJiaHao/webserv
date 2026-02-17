/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: Ho Wai Keong <hwai_keo@student.42kl.edu    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/17 22:23:54 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/02/17 23:05:50 by Ho Wai Keon      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Webserv.hpp"
#include "DummyConfig.hpp"

int	main(int argc, char **argv)
{
	if (argc == 1 || argc == 2)
	{
		try
		{
			std::string			RawConfig; // for later
			DummyConfig			DummyConfigChunk;
			
			
		}
		catch (std::exception &e)
		{
			std::cerr << e.what() << std::endl;
			return (1);
		}
	}
	else
	{
		std::cerr << "problem argument " << std::endl;
		return (1);
	}
	return (0);
}

// the config is comes from argv