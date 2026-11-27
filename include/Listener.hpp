#ifndef LISTENER_HPP
#define LISTENER_HPP

#include "ServerConfig.hpp"
#include <vector>
#include <map>

// Opens a non-blocking, SO_REUSEADDR listening socket for every listen entry
// of every server. Returns fd -> the server it belongs to (one entry per fd).
// On failure, closes all sockets opened so far and throws std::runtime_error
// with host:port and the reason.
// servers must outlive the map and never be resized (the pointers point into it).
// Collision checks are left to A8.
std::map<int, const ServerConfig*> openListeners(const std::vector<ServerConfig> &servers);

#endif