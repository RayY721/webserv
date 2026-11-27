#ifndef REQUEST_HANDLER_HPP
#define REQUEST_HANDLER_HPP

#include "Request.hpp"
#include "Response.hpp"
#include "ServerConfig.hpp"

// What A (A5) calls when parseRequestHead says Done: the Response for that request.
// server: the config of the server this client belongs to (its root, locations...).
// For now it always answers a small page; B6 will find and send the real file.
Response handleRequest(const Request& req, const ServerConfig& server);

// What A (A5) calls when parseRequestHead says Error, or for any other error:
// a Response with that status and its error page.
// For now it is a small default page; B7 will use the error_page of the config.
Response errorResponse(int status, const ServerConfig& server);

#endif