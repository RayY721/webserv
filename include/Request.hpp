#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <map>
#include <string>

// The methods webserv handles. There are only a few, so each one has its own value.
// Any other method (PUT, FOO...) becomes Unknown, and webserv answers 405.
enum class Method
{
	Get,
	Post,
	Delete,
	Unknown
};

// One request from the client, split into its parts.
// The client asks for something, so a request has a method and a path.
// Filled in by the request parser (part B).
// Example: "GET /search?q=cats HTTP/1.1"
//   method = Get, path = "/search", query = "q=cats", version = "HTTP/1.1"
struct Request
{
	// Starts as Unknown, like an int that starts at 0, until the parser reads the method.
	Method method = Method::Unknown;

	// What the client asks for, without the part after '?'.
	std::string path;

	// The part after '?', without the '?'. Empty if there is no '?'.
	std::string query;

	std::string version;

	// Name -> value pairs, e.g. "host" -> "localhost".
	// Names are saved in lowercase ("Host" -> "host"), so always look them up in lowercase.
	std::map<std::string, std::string> headers;

	// What comes after the empty line, e.g. an uploaded file.
	// It can contain '\0', so add data with its size, body.append(data, size), never as a char*.
	std::string body;
};

#endif
