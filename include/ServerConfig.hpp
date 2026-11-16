#ifndef SERVER_CONFIG_HPP
#define SERVER_CONFIG_HPP

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "LocationConfig.hpp"

struct ListenAddress
{
	std::string address;
	unsigned short port;

	ListenAddress(const std::string &host, unsigned short number)
		: address(host), port(number) {}
};

// One server block. Defaults are provisional until the C1 configuration
// contract is approved.
// listen starts empty; after a server block is parsed, the parser adds
// 127.0.0.1:8080 if no listen directive was given.
class ServerConfig
{
public:
	std::vector<ListenAddress> listen;
	std::string root;
	std::string index;
	std::map<int, std::string> error_page; // Status code -> file path.
	std::size_t client_max_body_size;
	std::vector<LocationConfig> locations;

	// Constructor
	ServerConfig()
		: root("."), index("index.html"), client_max_body_size(1024 * 1024) {}
};

#endif
