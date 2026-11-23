#include "RequestParser.hpp"

#include <cctype>

// Turns the method text into the enum.
// Any other text (PUT, FOO, get...) becomes Unknown, and B5 rejects it.
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

// The version must look like HTTP/<digit>.<digit>, e.g. "HTTP/1.1".
// Upper case only: "http/1.1" is refused.
// Whether that version is supported (1.0 and 1.1, otherwise 505) is checked later.
static bool looksLikeVersion(const std::string& version)
{
	return version.size() == 8
		&& version.substr(0, 5) == "HTTP/"
		&& std::isdigit(static_cast<unsigned char>(version[5]))
		&& version[6] == '.'
		&& std::isdigit(static_cast<unsigned char>(version[7]));
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
	// The path always starts with '/': "GET index.html HTTP/1.1" is refused.
	if (target[0] != '/')
		return false;
	if (!looksLikeVersion(version))
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
