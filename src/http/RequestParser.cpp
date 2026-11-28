#include "RequestParser.hpp"

#include <cctype>
#include <vector>

// Every rule here was checked against NGINX 1.24 with tests/compare_nginx.py.

// The longest first line we accept; longer gives 414 URI Too Long (8 KB, like NGINX).
static const std::size_t MAX_REQUEST_LINE = 8192;
// The longest header line we accept; longer gives 400 (8 KB, like NGINX).
static const std::size_t MAX_HEADER_LINE = 8192;
// The longest head (first line + headers) we accept; longer gives 400 (32 KB, like NGINX).
static const std::size_t MAX_HEAD = 32768;
// The answer to a method webserv does not know (PUT, FOO...): 405 like NGINX (the RFC says 501).
static const int UNKNOWN_METHOD_STATUS = 405;

// Turns the method text into the enum.
// Any other well-formed method (PUT, FOO...) becomes Unknown, and parseRequestHead answers 405.
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

// A method may only have A-Z, '_' and '-', like in NGINX: "get" or "G3T" is not a method.
static bool isMethodName(const std::string& text)
{
	for (char c : text)
		if (!(c >= 'A' && c <= 'Z') && c != '_' && c != '-')
			return false;
	return true;
}

static bool isDigits(const std::string& text)
{
	if (text.empty())
		return false;
	for (char c : text)
		if (!std::isdigit(static_cast<unsigned char>(c)))
			return false;
	return true;
}

// The major number of "HTTP/<major>.<minor>", like NGINX:
// 1 for HTTP/1.x, 2 for any higher major (HTTP/2.0, HTTP/12.1), 0 if the major starts with 0 (HTTP/0.9, HTTP/01.1)
// and -1 if the text does not look like a version ("FOO", "http/1.1", "HTTP/1", "HTTP/1.1000").
static int versionMajor(const std::string& version)
{
	if (version.substr(0, 5) != "HTTP/")
		return -1;
	std::size_t dot = version.find('.', 5);
	if (dot == std::string::npos)
		return -1;
	std::string major = version.substr(5, dot - 5);
	std::string minor = version.substr(dot + 1);
	if (!isDigits(major) || !isDigits(minor))
		return -1;
	// Leading zeros do not count ("0999" is 999). Higher than 999 is refused, like NGINX.
	std::size_t firstDigit = minor.find_first_not_of('0');
	if (firstDigit != std::string::npos && minor.size() - firstDigit > 3)
		return -1;
	// A major that starts with 0 ("0", "01") is refused with 400, like NGINX.
	if (major[0] == '0')
		return 0;
	if (major == "1")
		return 1;
	return 2;
}

// True for "HTTP/1.0", "HTTP/1.00"...: the minor is only zeros.
static bool versionMinorIsZero(const std::string& version)
{
	std::string minor = version.substr(version.find('.') + 1);
	return minor.find_first_not_of('0') == std::string::npos;
}

// "http", "https", "h2"...: a letter, then letters, digits, '+', '-' or '.', like NGINX.
static bool isScheme(const std::string& text)
{
	if (text.empty() || !std::isalpha(static_cast<unsigned char>(text[0])))
		return false;
	for (char c : text)
		if (!std::isalnum(static_cast<unsigned char>(c)) && c != '+' && c != '-' && c != '.')
			return false;
	return true;
}

// The host of a full URL, with its port if there is one, as NGINX accepts it:
// a name ("a.b-c") or an address in brackets ("[::1]"), then maybe ':' and digits ("a:80").
static bool isUrlHost(const std::string& text)
{
	std::size_t i = 0;
	if (!text.empty() && text[0] == '[')
	{
		std::size_t close = text.find(']');
		if (close == std::string::npos)
			return false;
		for (i = 1; i < close; i++)
			if (!std::isalnum(static_cast<unsigned char>(text[i]))
				&& std::string("!$&'()*+,-.:;=_~").find(text[i]) == std::string::npos)
				return false;
		i = close + 1;
	}
	else
	{
		while (i < text.size() && (std::isalnum(static_cast<unsigned char>(text[i]))
				|| text[i] == '.' || text[i] == '-'))
			i++;
		if (i == 0 || text.substr(0, i).find("..") != std::string::npos)
			return false;
	}
	if (i == text.size())
		return true;
	if (text[i] != ':')
		return false;
	return text.find_first_not_of("0123456789", i + 1) == std::string::npos;
}

// The target is a path ("/a?x=1"), or a full URL ("http://host/a?x=1"), which NGINX accepts too.
// From a full URL only the part from the first '/' or '?' after the host is kept
// ("/" if there is none, and "/?x=1" for "http://host?x=1").
// Returns false for anything else ("index.html", "*", "http://").
static bool targetToPath(const std::string& target, std::string& path)
{
	if (target[0] == '/')
	{
		path = target;
		return true;
	}
	std::size_t scheme = target.find("://");
	if (scheme == std::string::npos || scheme == 0)
		return false;
	if (!isScheme(target.substr(0, scheme)))
		return false;
	std::size_t host = scheme + 3;
	// The host ends at the first '/' or '?' after it.
	std::size_t end = target.find_first_of("/?", host);
	if (!isUrlHost(target.substr(host, end - host)))
		return false;
	if (end == std::string::npos)
		path = "/";
	else if (target[end] == '?')
		path = "/" + target.substr(end);	// "http://a?x=1" -> "/?x=1"
	else
		path = target.substr(end);
	return true;
}

// Splits the line at the spaces: "GET  /a   HTTP/1.1 " -> "GET", "/a", "HTTP/1.1".
// Several spaces between the parts and spaces at the end are fine, like NGINX.
static std::vector<std::string> splitAtSpaces(const std::string& line)
{
	std::vector<std::string> parts;
	std::size_t start = 0;
	while (start < line.size())
	{
		if (line[start] == ' ')
		{
			start++;
			continue;
		}
		std::size_t space = line.find(' ', start);
		if (space == std::string::npos)
			space = line.size();
		parts.push_back(line.substr(start, space - start));
		start = space;
	}
	return parts;
}

bool parseRequestLine(const std::string& line, Request& req)
{
	// A space at the start is refused, like NGINX.
	if (line.empty() || line[0] == ' ')
		return false;
	// The line has three parts: METHOD TARGET VERSION.
	std::vector<std::string> parts = splitAtSpaces(line);
	if (parts.size() != 3)
		return false;
	const std::string& method = parts[0];
	const std::string& version = parts[2];
	// "get" or "G3T" is badly written (400), not an unknown method (405).
	if (!isMethodName(method))
		return false;
	std::string target;
	if (!targetToPath(parts[1], target))
		return false;
	// Everything from '#' on is for the browser, not the server: NGINX drops it.
	target = target.substr(0, target.find('#'));
	// HTTP/0.x is refused here (400). HTTP/2.0 and higher is well written: 505 comes later.
	if (versionMajor(version) < 1)
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

// True if text has a control character (0-31 or 127), a tab included.
static bool hasControlChar(const std::string& text)
{
	for (char c : text)
	{
		unsigned char u = static_cast<unsigned char>(c);
		if (u < 32 || u == 127)
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

// "Host: localhost" -> req.headers["host"] = "localhost". Like NGINX:
// - a line with no ':' is a header with an empty value ("Host" alone is an empty Host: 400);
// - a name with other characters than letters, digits and '-' ("X(y", "X_Y", "X.Y") is ignored;
// - an empty name, a space, tab or control character in the name ("Host : a"), a '\0' or '\r'
//   in the value, or a second Host, Content-Length or Transfer-Encoding gives false (400).
static bool parseHeaderLine(const std::string& line, Request& req)
{
	std::size_t colon = line.find(':');
	std::string name = line.substr(0, colon);
	if (name.empty() || name.find(' ') != std::string::npos || hasControlChar(name))
		return false;
	std::string value;		// Empty if the line has no ':'.
	if (colon != std::string::npos)
		value = trim(line.substr(colon + 1));
	if (value.find('\0') != std::string::npos || value.find('\r') != std::string::npos)
		return false;
	name = toLower(name);
	for (char c : name)
		if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-')
			return true;
	if (mustBeUnique(name) && req.headers.count(name) > 0)
		return false;
	req.headers[name] = value;
	return true;
}

// A Host value NGINX refuses with 400: empty, ".", with a space or a control character
// (a tab included), with a '/', or with "..".
static bool isBadHost(const std::string& host)
{
	return host.empty() || host == "." || host.find(' ') != std::string::npos
		|| hasControlChar(host) || host.find('/') != std::string::npos
		|| host.find("..") != std::string::npos;
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
			std::size_t waiting = buffer.size() - start;
			if (lines.empty() && waiting > MAX_REQUEST_LINE + 1)
			{
				errorStatus = 414;
				return ParseStatus::Error;
			}
			if ((!lines.empty() && waiting > MAX_HEADER_LINE + 1) || buffer.size() > MAX_HEAD)
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
		if ((!lines.empty() && line.size() > MAX_HEADER_LINE) || start > MAX_HEAD)
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
	// No control character in the first line, a tab included: C functions like open()
	// stop at '\0', so "/a\0.py" would look like a .py to us but open "/a".
	if (hasControlChar(lines[0]) || !parseRequestLine(lines[0], parsed))
	{
		errorStatus = 400;
		return ParseStatus::Error;
	}
	// Only HTTP/1.x is supported: HTTP/2.0 and higher give 505. Checked here, before
	// the headers, as NGINX does.
	if (versionMajor(parsed.version) != 1)
	{
		errorStatus = 505;
		return ParseStatus::Error;
	}
	// From here the version is HTTP/1.x. Keep only "HTTP/1.0" or "HTTP/1.1", like NGINX:
	// "HTTP/1.00" is HTTP/1.0, and "HTTP/1.5" is treated as HTTP/1.1.
	if (versionMinorIsZero(parsed.version))
		parsed.version = "HTTP/1.0";
	else
		parsed.version = "HTTP/1.1";
	for (std::size_t i = 1; i < lines.size(); i++)
	{
		if (!parseHeaderLine(lines[i], parsed))
		{
			errorStatus = 400;
			return ParseStatus::Error;
		}
	}

	// 3. Host: required from HTTP/1.1 on (HTTP/1.0 does not need it), and never invalid.
	bool hasHost = parsed.headers.count("host") > 0;
	if ((!hasHost && parsed.version != "HTTP/1.0") || (hasHost && isBadHost(parsed.headers.at("host"))))
	{
		errorStatus = 400;
		return ParseStatus::Error;
	}

	// A method webserv does not know: 405, after the checks above, like NGINX
	// (so "FOO / HTTP/1.1" without Host still gives 400).
	if (parsed.method == Method::Unknown)
	{
		errorStatus = UNKNOWN_METHOD_STATUS;
		return ParseStatus::Error;
	}

	// 4. Everything is right: fill req and remove the head from buffer.
	// What is left in buffer came after the empty line.
	req = parsed;
	buffer.erase(0, start);
	return ParseStatus::Done;
}
