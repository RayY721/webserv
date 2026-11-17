#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include <map>
#include <string>

// One response from webserv, split into its parts.
// The server says how it went, so a response has a status code instead of a method and a path.
// Part B fills it in, part A sends it to the client.
// Example: a missing page gives status = 404, "Content-Type" -> "text/html", body = the error page.
struct Response
{
	// Starts at 200 (OK), like an int that starts at 0, and changes if something goes wrong.
	// Only the number is saved: the text ("OK", "Not Found") is added when sending.
	int status = 200;

	// Name -> value pairs, saved as they are sent, e.g. "Content-Type" -> "text/html".
	// Content-Length is not saved: it is calculated from the body when sending.
	std::map<std::string, std::string> headers;

	// What the client will see, e.g. a page or an image.
	// It can contain '\0', so add data with its size, body.append(data, size), never as a char*.
	std::string body;

	// For big files: where the file is on disk, so part A sends it in pieces
	// instead of loading it all into body. Empty: send body.
	std::string filePath;
};

#endif