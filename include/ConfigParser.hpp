#ifndef CONFIG_PARSER_HPP
#define CONFIG_PARSER_HPP

#include <vector>

#include "ConfigLexer.hpp"
#include "ServerConfig.hpp"

// Builds one ServerConfig per server block, in file order.
// Throws ConfigError with the line of the offending token.
// Location blocks are only skipped for now; C4b fills them in.
std::vector<ServerConfig> parseConfig(const std::vector<Token> &tokens);

#endif
