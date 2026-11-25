#ifndef REQUEST_PARSER_HPP
#define REQUEST_PARSER_HPP

#include <string>

#include "Request.hpp"

// Reads the first line of a request and fills method, path, query and version.
// Example: "GET /search?q=cats HTTP/1.1"
//   method = Get, path = "/search", query = "q=cats", version = "HTTP/1.1"
// The line comes without "\r\n" at the end: parseRequestHead cuts the lines and calls this function.
// Returns false (later 400 Bad Request) if the line is not three parts separated
// by one space each, if the path does not start with '/', or if the version
// does not look like HTTP/<digit>.<digit>.
bool parseRequestLine(const std::string& line, Request& req);

// What parseRequestHead says after looking at the bytes received so far.
enum class ParseStatus
{
	NeedMore,	// The empty line has not arrived yet: wait for more bytes.
	Done,		// The head is complete and req is filled.
	Error		// The head is wrong: errorStatus says which code to answer.
};

// Looks in buffer (the bytes received from the client) for a complete head:
// the first line, the headers and the empty line that ends them.
// Lines may end with "\r\n" or with "\n" alone.
// Done: fills req (header names in lowercase) and removes the head from buffer,
//   so what is left in buffer is whatever came after the empty line.
// NeedMore: changes nothing; call it again when more bytes arrive.
// Error: changes nothing in req; errorStatus is 414 (first line too long) or 400 (anything else).
// Known limitation: empty lines before the first line count toward the 32 KB limit,
//   so a client that sends only empty lines gets 400 when it reaches 32 KB.
ParseStatus parseRequestHead(std::string& buffer, Request& req, int& errorStatus);

#endif
