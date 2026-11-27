#ifndef REQUEST_PARSER_HPP
#define REQUEST_PARSER_HPP

#include <string>

#include "Request.hpp"

// Reads the first line of a request and fills method, path, query and version.
// Example: "GET /search?q=cats HTTP/1.1"
//   method = Get, path = "/search", query = "q=cats", version = "HTTP/1.1"
// The line comes without "\r\n" at the end: parseRequestHead cuts the lines and calls this function.
// Returns false (later 400 Bad Request) if the line is not three parts separated by spaces,
// if the method has other characters than A-Z, '_' and '-', if the target is not a path or a
// full URL, or if the version is not HTTP/<digits>.<digits> or is HTTP/0.x. All like NGINX.
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
// Error: changes nothing in req; errorStatus is 414 (first line too long), 505 (HTTP/2.0 or
//   higher), 405 (unknown method, e.g. PUT or FOO) or 400 (anything else, e.g. HTTP/1.1
//   without Host).
// Known limitation: empty lines before the first line count toward the 32 KB limit,
//   so a client that sends only empty lines gets 400 when it reaches 32 KB.
ParseStatus parseRequestHead(std::string& buffer, Request& req, int& errorStatus);

#endif
