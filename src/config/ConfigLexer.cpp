#include "ConfigLexer.hpp"

#include <cctype>
#include <cstddef>

#include "ConfigError.hpp"

static bool isSpecial(char c)
{
	return c == '{' || c == '}' || c == ';' || c == '#' || c == '"' || c == '\'';
}

static Token::Type specialType(char c)
{
	if (c == '{')
		return Token::OpenBrace;
	if (c == '}')
		return Token::CloseBrace;
	return Token::Semicolon;
}

std::vector<Token> tokenize(const std::string &content)
{
	std::vector<Token> tokens;
	int line = 1;
	std::size_t i = 0;

	while (i < content.size())
	{
		char c = content[i];
		if (c == '\n')
		{
			++line;
			++i;
		}
		else if (std::isspace(static_cast<unsigned char>(c)))
			++i;
		else if (c == '#')
		{
			while (i < content.size() && content[i] != '\n')
				++i;
		}
		else if (c == '"' || c == '\'')
			throw ConfigError(line, "quoted strings are not supported");
		else if (c == '{' || c == '}' || c == ';')
		{
			tokens.push_back(Token{specialType(c), std::string(1, c), line});
			++i;
		}
		else
		{
			std::size_t start = i;
			while (i < content.size()
				&& !std::isspace(static_cast<unsigned char>(content[i]))
				&& !isSpecial(content[i]))
				++i;
			tokens.push_back(Token{Token::Word, content.substr(start, i - start), line});
		}
	}
	return tokens;
}
