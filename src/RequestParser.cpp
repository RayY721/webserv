#include "RequestParser.hpp"

// Turns the method text into the enum.
// Any other text (PUT, FOO, get...) becomes Unknown, and webserv answers 405 (B5).
static Method toMethod(const std::string& text)
{
	if (text == "GET")
		return Method::Get;
	if (text == "POST")
		return Method::Post;
	if (text == "DELETE")
		return Method::Delete;
	return Method::Unknown;
}

bool parseRequestLine(const std::string& line, Request& req)
{
	// The line has three parts separated by one space each:
	// METHOD space TARGET space VERSION
	std::size_t firstSpace = line.find(' ');
	if (firstSpace == std::string::npos)
		return false;
	std::size_t secondSpace = line.find(' ', firstSpace + 1);
	if (secondSpace == std::string::npos)
		return false;
	// A third space means more than three parts.
	if (line.find(' ', secondSpace + 1) != std::string::npos)
		return false;

	std::string method = line.substr(0, firstSpace);
	std::string target = line.substr(firstSpace + 1, secondSpace - firstSpace - 1);
	std::string version = line.substr(secondSpace + 1);
	// Two spaces together leave an empty part, e.g. "GET  HTTP/1.1".
	if (method.empty() || target.empty() || version.empty())
		return false;

	req.method = toMethod(method);
	req.version = version;

	// The target is the path, plus the query after the first '?' if there is one.
	std::size_t question = target.find('?');
	if (question == std::string::npos)
	{
		req.path = target;
		req.query = "";
	}
	else
	{
		req.path = target.substr(0, question);
		req.query = target.substr(question + 1);
	}
	return true;
}
