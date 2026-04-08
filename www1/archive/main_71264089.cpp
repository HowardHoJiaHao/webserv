/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aaissa <aaissa@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/05 06:43:49 by aaissa            #+#    #+#             */
/*   Updated: 2026/03/05 06:43:50 by aaissa           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */


#include <iostream>
#include "RPN.hpp"

int main(int ac, char **av)
{
	if (ac != 2) {
		std::cerr << "Error: one argument needed!" << std::endl;
		return (1);
	}


	try {
		std::string input(av[1]);
		RPN rpn(input);
		std::cout << rpn.getResult() << std::endl;
	}
	catch(const std::exception& e) {
		std::cerr << e.what() << '\n';
	}
	return (0);
}