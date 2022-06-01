/*
 * Physically Based Rendering
 * Forked from Michał Siejak PBR project
 */

#include <fstream>
#include <sstream>
#include <memory>

#if _WIN32
#include <windows.h>
#endif // _WIN32

#include "utils.hpp"

std::string File::readText(const std::string& filename)
{
	std::ifstream file{filename};
	if(!file.is_open())
	{
		throw std::runtime_error("Could not open file: " + filename);
	}

	std::stringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}
	
std::vector<char> File::readBinary(const std::string& filename)
{
	std::ifstream file{filename, std::ios::binary | std::ios::ate};
	if(!file.is_open())
	{
		throw std::runtime_error("Could not open file: " + filename);
	}

	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);

	std::vector<char> buffer(size);
	file.read(buffer.data(), size);
	return buffer;
}
