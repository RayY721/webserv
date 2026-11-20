#ifndef SERVER_CONFIG_HPP
#define SERVER_CONFIG_HPP

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "LocationConfig.hpp"

// One listen directive. address is a dotted IPv4 address or "localhost";
// a bare "listen <port>;" stores "0.0.0.0". The parser accepts only decimal
// ports in 1-65535 and checks the range before narrowing to unsigned short.
// A resolves the address with getaddrinfo and checks endpoint collisions (A8).
struct ListenAddress
{
	std::string address;
	unsigned short port;

	ListenAddress(const std::string &host, unsigned short number)
		: address(host), port(number) {}
};

// One server block. Defaults follow the C1 contract submitted for review.
//
// Parser obligations:
// - listen starts empty; the parser must reject a server block without an
//   explicit listen directive. It must not insert a fallback listener.
// - Relative root (including the default ".") and error_page paths are
//   resolved against the config file directory before handing configs to
//   runtime code. Absolute paths stay absolute.
// - If no location has path "/", the parser appends a default LocationConfig
//   (path "/", empty root/index so it uses the server's values, GET only).
//   Every URI then matches at least one location.
//
// Lifetime: all ServerConfig objects (and their locations) are built once by
// the parser and never modified, added to, or resized afterwards. Client keeps
// a const ServerConfig*, and route code may keep pointers into locations, so
// any reallocation after startup would leave those pointers dangling.
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
