#ifndef EVENTLOOP_HPP
#define EVENTLOOP_HPP

#include <map>
#include "ServerConfig.hpp"

// Runs till the process is killed, watching every listener for new connections
// and every client for whatever its state needs (pollEvents()).
// The caller opened listener fds and still owns them. The loop only accepts on them, never closes them.
// If handling one client throws (including std::bad_alloc), only that client is closed. The loop continues serving.
// Ignores SIGPIPE, so a send to a disconnected client returns -1 instead of killing the server.
void runEventLoop(const std::map<int, const ServerConfig*> &listeners);

#endif