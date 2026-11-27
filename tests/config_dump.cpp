// Test helper, not part of webserv: prints what the config layer reads,
// so tests/test_config.py can check it.
//   ./config_dump tokens <file>
//   ./config_dump servers <file>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "ConfigError.hpp"
#include "ConfigLexer.hpp"
#include "ConfigParser.hpp"

static const char *typeName(Token::Type type)
{
	switch (type)
	{
	case Token::Word:
		return "Word";
	case Token::OpenBrace:
		return "OpenBrace";
	case Token::CloseBrace:
		return "CloseBrace";
	case Token::Semicolon:
		return "Semicolon";
	}
	return "?";
}

static bool readFile(const std::string &path, std::string &content)
{
	std::ifstream file(path.c_str());
	if (!file)
		return false;
	std::ostringstream buffer;
	buffer << file.rdbuf();
	content = buffer.str();
	return true;
}

static void dumpTokens(const std::string &content)
{
	std::vector<Token> tokens = tokenize(content);
	for (std::size_t i = 0; i < tokens.size(); ++i)
		std::cout << tokens[i].line << '\t' << typeName(tokens[i].type)
				  << '\t' << tokens[i].text << '\n';
}

static void dumpServers(const std::string &content)
{
	std::vector<ServerConfig> servers = parseConfig(tokenize(content));
	for (std::size_t i = 0; i < servers.size(); ++i)
	{
		const ServerConfig &server = servers[i];
		std::cout << "server\n";
		for (std::size_t j = 0; j < server.listen.size(); ++j)
			std::cout << "listen " << server.listen[j].address << ' '
					  << server.listen[j].port << '\n';
		std::cout << "root " << server.root << '\n';
		std::cout << "index " << server.index << '\n';
		std::map<int, std::string>::const_iterator it;
		for (it = server.error_page.begin(); it != server.error_page.end(); ++it)
			std::cout << "error_page " << it->first << ' ' << it->second << '\n';
		std::cout << "client_max_body_size " << server.client_max_body_size << '\n';
	}
}

int main(int argc, char **argv)
{
	std::string mode = argc == 3 ? argv[1] : "";
	if (mode != "tokens" && mode != "servers")
	{
		std::cerr << "Usage: ./config_dump tokens|servers <file>" << std::endl;
		return 2;
	}
	std::string content;
	if (!readFile(argv[2], content))
	{
		std::cerr << "cannot read " << argv[2] << std::endl;
		return 1;
	}
	try
	{
		if (mode == "tokens")
			dumpTokens(content);
		else
			dumpServers(content);
	}
	catch (const ConfigError &e)
	{
		std::cerr << "line " << e.line() << ": " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
