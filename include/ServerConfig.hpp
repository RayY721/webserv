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

// One server block. Defaults follow the C1 contract submitted for review.
// listen starts empty; the parser must reject a server block without an
// explicit listen directive. It must not insert a fallback listener.
// The parser resolves relative root paths (including the default ".")
// against the config file directory before handing configs to runtime code.
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
