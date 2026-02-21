/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   engine.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: Ho Wai Keong <hwai_keo@student.42kl.edu    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/19 20:53:32 by Ho Wai Keon       #+#    #+#             */
/*   Updated: 2026/02/19 22:00:54 by Ho Wai Keon      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Config.hpp"
#include <map>
#include <utility>
#include <string>
#include "Connection.hpp"

#ifndef ENGINE_HPP
#define ENGINE_HPP

class Engine
{
	private:
		const Config& _config;
		std::map<std::pair<std::string,int>, int> _listenSockets;
		std::map<int, Connection*> _connections;

	public:
		Engine(const Config& _config);
		~Engine();
		void	setupListeningSockets();
		void	run();
		std::string buildMinimalResponse();
};

#endif