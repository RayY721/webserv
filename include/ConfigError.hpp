#ifndef CONFIG_ERROR_HPP
#define CONFIG_ERROR_HPP

#include <stdexcept>
#include <string>

// The single error type of the config layer. The lexer and the parser throw it
// with the line of the offending token; main (C5) prints "path:line: reason".
class ConfigError : public std::runtime_error
{
public:
	ConfigError(int line, const std::string &message)
		: std::runtime_error(message), line_(line) {}

	int line() const { return line_; }

private:
	int line_;
};

#endif
