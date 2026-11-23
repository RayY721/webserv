#ifndef CONFIG_LEXER_HPP
#define CONFIG_LEXER_HPP

#include <string>
#include <vector>

struct Token
{
	enum Type
	{
		Word,
		OpenBrace,
		CloseBrace,
		Semicolon
	};

	Type type;
	std::string text;
	int line;
};

// Splits config text into words, '{', '}' and ';', each with its line number.
// '#' starts a comment up to the end of the line.
// Throws ConfigError on a quote, because quoting is not supported.
std::vector<Token> tokenize(const std::string &content);

#endif
