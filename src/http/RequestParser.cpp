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

// Header names may only have letters, digits and !#$%&'*+-.^_`|~ (an HTTP "token").
static bool isNameChar(char c)
{
	return std::isalnum(static_cast<unsigned char>(c))
		|| std::string("!#$%&'*+-.^_`|~").find(c) != std::string::npos;
}

// True if text has a control character (like '\r' or '\0'). A tab is allowed.
static bool hasControlChar(const std::string& text)
{
	for (char c : text)
	{
		unsigned char u = static_cast<unsigned char>(c);
		if ((u < 32 && c != '\t') || u == 127)
			return true;
	}
	return false;
}

// Headers that may appear only once: with two different values we would not know
// which one is true (Content-Length: 5 and Content-Length: 50).
static bool mustBeUnique(const std::string& name)
{
	return name == "host" || name == "content-length" || name == "transfer-encoding";
}

// "Host: localhost" -> req.headers["host"] = "localhost".
// Returns false if there is no ':', if the name is empty or has a character that
// is not allowed, if the value has a control character, or if the header must be
// unique and is already there.
static bool parseHeaderLine(const std::string& line, Request& req)
{
	std::size_t colon = line.find(':');
	if (colon == std::string::npos || colon == 0)
		return false;
	std::string name = toLower(line.substr(0, colon));
	for (char c : name)
		if (!isNameChar(c))
			return false;
	std::string value = trim(line.substr(colon + 1));
	if (hasControlChar(value))
		return false;
	if (mustBeUnique(name) && req.headers.count(name) > 0)
		return false;
	req.headers[name] = value;
	return true;
}

ParseStatus parseRequestHead(std::string& buffer, Request& req, int& errorStatus)
{
	// 1. Cut the buffer into lines until the empty line.
	std::vector<std::string> lines;	// lines[0] = request line, then one header per entry.
	std::size_t start = 0;			// Where the next line starts in buffer.
	while (true)
	{
		std::size_t end = buffer.find('\n', start);
		if (end == std::string::npos)
		{
			// This line has not finished arriving. Wait, unless it is already too long.
			// + 1: the '\r' of its "\r\n" may already be here, and it is not part of the line.
			if (lines.empty() && buffer.size() - start > MAX_REQUEST_LINE + 1)
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

	// 2. The first line, then each header, into a new Request.
	// It is copied to req only if everything is right, so on Error req does not change.
	Request parsed;
	if (!parseRequestLine(lines[0], parsed))
	{
		errorStatus = 400;
		return ParseStatus::Error;
	}
	for (std::size_t i = 1; i < lines.size(); i++)
	{
		if (!parseHeaderLine(lines[i], parsed))
		{
			errorStatus = 400;
			return ParseStatus::Error;
		}
	}

	// 3. Everything is right: fill req and remove the head from buffer.
	// What is left in buffer came after the empty line.
	req = parsed;
	buffer.erase(0, start);
	return ParseStatus::Done;
}
