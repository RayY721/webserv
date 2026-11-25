#include "RequestParser.hpp"

#include <cctype>
#include <vector>

// The longest first line we accept; longer gives 414 URI Too Long (8 KB, like NGINX).
static const std::size_t MAX_REQUEST_LINE = 8192;
// The longest head (first line + headers) we accept; longer gives 400 (32 KB, like NGINX).
static const std::size_t MAX_HEAD = 32768;

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

// Cuts the line from start up to the '\n' at end, without the '\n'
// and without the '\r' before it, if there is one.
static std::string cutLine(const std::string& buffer, std::size_t start, std::size_t end)
{
	std::string line = buffer.substr(start, end - start);
	if (!line.empty() && line.back() == '\r')
		line.pop_back();
	return line;
}

// "Host" -> "host"
static std::string toLower(std::string text)
{
	for (char& c : text)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return text;
}

// Removes the spaces and tabs at both ends: "  localhost " -> "localhost"
static std::string trim(const std::string& text)
{
	std::size_t first = text.find_first_not_of(" \t");
	if (first == std::string::npos)
		return "";
	std::size_t last = text.find_last_not_of(" \t");
	return text.substr(first, last - first + 1);
}

// "Host: localhost" -> req.headers["host"] = "localhost".
// Returns false if there is no ':', or if the name is empty or has a space or tab in it.
static bool parseHeaderLine(const std::string& line, Request& req)
{
	std::size_t colon = line.find(':');
	if (colon == std::string::npos || colon == 0)
		return false;
	std::string name = line.substr(0, colon);
	if (name.find_first_of(" \t") != std::string::npos)
		return false;
	req.headers[toLower(name)] = trim(line.substr(colon + 1));
	return true;
}

ParseStatus parseRequestHead(std::string& buffer, Request& req, int& errorStatus)
{
	// 1. Cut the buffer into lines until the empty line.
	std::vector<std::string> lines;	// The first line, then one line per header.
	std::size_t start = 0;			// Where the next line starts in buffer.
	while (true)
	{
		std::size_t end = buffer.find('\n', start);
		if (end == std::string::npos)
		{
			// This line has not finished arriving. Wait, unless it is already too long.
			if (lines.empty() && buffer.size() - start > MAX_REQUEST_LINE)
			{
				errorStatus = 414;
				return ParseStatus::Error;
			}
			if (buffer.size() > MAX_HEAD)
			{
				errorStatus = 400;
				return ParseStatus::Error;
			}
			return ParseStatus::NeedMore;
		}
		std::string line = cutLine(buffer, start, end);
		start = end + 1;
		if (lines.empty() && line.size() > MAX_REQUEST_LINE)
		{
			errorStatus = 414;
			return ParseStatus::Error;
		}
		if (start > MAX_HEAD)
		{
			errorStatus = 400;
			return ParseStatus::Error;
		}
		if (line.empty())
		{
			// Empty lines before the first line are skipped, like NGINX does.
			if (lines.empty())
				continue;
			break;	// The empty line after the headers: the head is complete.
		}
		lines.push_back(line);
	}

	// 2. The first line, then each header.
	if (!parseRequestLine(lines[0], req))
	{
		errorStatus = 400;
		return ParseStatus::Error;
	}
	for (std::size_t i = 1; i < lines.size(); i++)
	{
		if (!parseHeaderLine(lines[i], req))
		{
			errorStatus = 400;
			return ParseStatus::Error;
		}
	}

	// 3. Remove the head from buffer. What is left came after the empty line.
	buffer.erase(0, start);
	return ParseStatus::Done;
}
