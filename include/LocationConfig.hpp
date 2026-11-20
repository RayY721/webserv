#ifndef LOCATION_CONFIG_HPP
#define LOCATION_CONFIG_HPP

#include <map>
#include <set>
#include <string>

#include "Method.hpp"

// One URL-prefix route. Defaults follow the C1 contract submitted for review.
// Empty root/index means the location uses the server's value.
//
// Parser obligations:
// - path is stored without a trailing '/', except "/" itself:
//   "location /uploads/" and "location /uploads" both store "/uploads".
//   B matches whole segments: "/uploads" matches "/uploads" and "/uploads/...".
// - methods holds only Get, Post and Delete. Any other word in a methods
//   directive (GETT, PUT, get...) is a config error with its line number;
//   Method::Unknown is never stored.
// - return_status is one of 301, 302, 303, 307, 308; any other code is a
//   config error.
// - Relative root, upload_store and cgi executable paths are resolved against
//   the config file directory; absolute paths stay absolute.
class LocationConfig
{
public:
	std::string path;
	std::string root;
	std::string index;
	std::set<Method> methods;
	bool autoindex;
	int return_status;                 // 0 means no redirect.
	std::string return_target;         // Empty when no redirect is configured.
	std::string upload_store;          // Empty means uploads are disabled.
	std::map<std::string, std::string> cgi; // Extension -> executable path.

	// Constructor:
	LocationConfig()
		: path("/"), methods{Method::Get}, autoindex(false), return_status(0) {}
};

#endif
