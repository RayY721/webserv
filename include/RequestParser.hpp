#ifndef REQUEST_PARSER_HPP
#define REQUEST_PARSER_HPP

#include <string>

#include "Request.hpp"

// Reads the first line of a request and fills method, path, query and version.
// Example: "GET /search?q=cats HTTP/1.1"
//   method = Get, path = "/search", query = "q=cats", version = "HTTP/1.1"
// The line comes without "\r\n" at the end: cutting the lines is part of B3.
// Returns false if the line is not three parts separated by one space each.
// Later (B4), false becomes 400 Bad Request.
bool parseRequestLine(const std::string& line, Request& req);

#endif
