#ifndef LOCATION_CONFIG_HPP
#define LOCATION_CONFIG_HPP

#include <map>
#include <set>
#include <string>

// One URL-prefix route. Empty root/index means inherit the server value.
// Defaults are provisional until the C1 configuration contract is approved.
class LocationConfig
{
public:
	std::string path;
	std::string root;
	std::string index;
	std::set<std::string> methods;
	bool autoindex;
	int return_status;                 // 0 means no redirect.
	std::string return_target;         // Empty when no redirect is configured.
	std::string upload_store;          // Empty means uploads are disabled.
	std::map<std::string, std::string> cgi; // Extension -> executable path.

	// Constructor:
	LocationConfig()
		: path("/"), autoindex(false), return_status(0)
	{
		methods.insert("GET");
	}
};

#endif
