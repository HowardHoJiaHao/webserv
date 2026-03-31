#include "engine_string_utils.hpp"

#include <cctype>

std::string toLowerAsciiEngine(const std::string& input)
{
	std::string lowered = input;
	for (size_t i = 0; i < lowered.size(); ++i)
		lowered[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lowered[i])));
	return lowered;
}

std::string trimAsciiEngine(const std::string& input)
{
	size_t start = 0;
	while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start])))
		++start;
	size_t end = input.size();
	while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1])))
		--end;
	return input.substr(start, end - start);
}
