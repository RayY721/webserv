// Test helper, not part of webserv: prints what the config layer reads,
// so tests/test_config.py can check it.
//   ./config_dump tokens <file>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "ConfigError.hpp"
#include "ConfigLexer.hpp"

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

int main(int argc, char **argv)
{
	if (argc != 3 || std::string(argv[1]) != "tokens")
	{
		std::cerr << "Usage: ./config_dump tokens <file>" << std::endl;
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
		dumpTokens(content);
	}
	catch (const ConfigError &e)
	{
		std::cerr << "line " << e.line() << ": " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
