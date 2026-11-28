// Test helper, not part of webserv: prints what the request parser reads,
// so tests/test_request_parser.py can check it.
//   ./request_dump <file>
// The file holds the raw bytes of a request, exactly as a client would send them.

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "RequestParser.hpp"

static const char *methodName(Method method)
{
	switch (method)
	{
	case Method::Get:
		return "Get";
	case Method::Post:
		return "Post";
	case Method::Delete:
		return "Delete";
	case Method::Unknown:
		return "Unknown";
	}
	return "?";
}

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		std::cerr << "Usage: ./request_dump <file>" << std::endl;
		return 2;
	}
	std::ifstream file(argv[1], std::ios::binary);
	if (!file)
	{
		std::cerr << "cannot read " << argv[1] << std::endl;
		return 1;
	}
	std::ostringstream bytes;
	bytes << file.rdbuf();
	std::string buffer = bytes.str();

	Request req;
	int status = 0;
	ParseStatus result = parseRequestHead(buffer, req, status);
	if (result == ParseStatus::NeedMore)
		std::cout << "need_more\n";
	else if (result == ParseStatus::Error)
		std::cout << "error " << status << '\n';
	else
	{
		std::cout << "done\n";
		std::cout << "method " << methodName(req.method) << '\n';
		std::cout << "path " << req.path << '\n';
		std::cout << "query " << req.query << '\n';
		std::cout << "version " << req.version << '\n';
		std::map<std::string, std::string>::const_iterator it;
		for (it = req.headers.begin(); it != req.headers.end(); ++it)
			std::cout << "header " << it->first << ": " << it->second << '\n';
		std::cout << "left " << buffer.size() << '\n';
	}
	return 0;
}
