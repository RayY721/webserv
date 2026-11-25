#include "ConfigParser.hpp"

#include <cctype>
#include <cstdlib>
#include <limits>
#include <set>
#include <string>

#include "ConfigError.hpp"

static int lastLine(const std::vector<Token> &tokens)
{
	return tokens.empty() ? 1 : tokens.back().line;
}

// Decimal digits only. Overflow is checked before each step, so a huge value
// is reported as an error instead of wrapping around.
static std::size_t parseNumber(const std::string &text, std::size_t max,
	int line, const std::string &what)
{
	if (text.empty())
		throw ConfigError(line, "invalid " + what + " '" + text + "'");
	std::size_t value = 0;
	for (std::size_t i = 0; i < text.size(); ++i)
	{
		if (!std::isdigit(static_cast<unsigned char>(text[i])))
			throw ConfigError(line, "invalid " + what + " '" + text + "'");
		std::size_t digit = text[i] - '0';
		if (value > (max - digit) / 10)
			throw ConfigError(line, what + " '" + text + "' is too large");
		value = value * 10 + digit;
	}
	return value;
}

// A byte count with an optional NGINX-style k/K or m/M suffix. The limit given
// to parseNumber is divided by the unit, so the multiplication cannot overflow.
static std::size_t parseBodySize(const std::string &text, int line)
{
	std::string digits = text;
	std::size_t unit = 1;
	if (!digits.empty())
	{
		char last = digits[digits.size() - 1];
		if (last == 'k' || last == 'K')
			unit = 1024;
		else if (last == 'm' || last == 'M')
			unit = 1024 * 1024;
		if (unit != 1)
			digits.erase(digits.size() - 1);
	}
	if (digits.empty())
		throw ConfigError(line, "invalid client_max_body_size '" + text + "'");
	std::size_t value = parseNumber(digits,
		std::numeric_limits<std::size_t>::max() / unit, line, "client_max_body_size");
	if (value == 0)
		throw ConfigError(line, "client_max_body_size must be at least 1");
	return value * unit;
}

static bool isIPv4(const std::string &text)
{
	int parts = 0;
	std::size_t start = 0;
	while (true)
	{
		std::size_t dot = text.find('.', start);
		std::string part = text.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
		if (part.empty() || part.size() > 3
			|| part.find_first_not_of("0123456789") != std::string::npos
			|| std::atoi(part.c_str()) > 255)
			return false;
		++parts;
		if (dot == std::string::npos)
			break;
		start = dot + 1;
	}
	return parts == 4;
}

// "<address>:<port>" or a bare "<port>", which listens on every interface.
static ListenAddress parseListen(const std::string &value, int line)
{
	std::string address = "0.0.0.0";
	std::string port = value;
	std::size_t colon = value.find(':');
	if (colon != std::string::npos)
	{
		address = value.substr(0, colon);
		port = value.substr(colon + 1);
		if (address != "localhost" && !isIPv4(address))
			throw ConfigError(line, "invalid listen address '" + address + "'");
	}
	std::size_t number = parseNumber(port, 65535, line, "port");
	if (number == 0)
		throw ConfigError(line, "port must be between 1 and 65535");
	return ListenAddress(address, static_cast<unsigned short>(number));
}

static bool isServerDirective(const std::string &name)
{
	return name == "listen" || name == "root" || name == "index"
		|| name == "error_page" || name == "client_max_body_size";
}

static bool isLocationDirective(const std::string &name)
{
	return name == "methods" || name == "autoindex" || name == "return"
		|| name == "upload_store" || name == "cgi";
}

// Collects the words after a directive name, up to and including its ';'.
static std::vector<std::string> readArgs(const std::vector<Token> &tokens,
	std::size_t &pos, const Token &name)
{
	std::vector<std::string> args;
	while (pos < tokens.size() && tokens[pos].type == Token::Word)
		args.push_back(tokens[pos++].text);
	if (pos >= tokens.size() || tokens[pos].type != Token::Semicolon)
		throw ConfigError(tokens[pos - 1].line, "expected ';' after " + name.text);
	++pos;
	return args;
}

static void expectArgs(const Token &name, const std::vector<std::string> &args,
	std::size_t count)
{
	if (args.size() != count)
		throw ConfigError(name.line, name.text + " expects " + std::to_string(count)
			+ (count == 1 ? " argument" : " arguments"));
}

static void applyServerDirective(ServerConfig &server, std::set<std::string> &seen,
	const Token &name, const std::vector<std::string> &args)
{
	if (name.text == "listen")
	{
		expectArgs(name, args, 1);
		server.listen.push_back(parseListen(args[0], name.line));
		return;
	}
	if (name.text == "error_page")
	{
		expectArgs(name, args, 2);
		int status = static_cast<int>(parseNumber(args[0], 599, name.line, "error_page status"));
		if (status < 400)
			throw ConfigError(name.line, "error_page status must be between 400 and 599");
		if (server.error_page.count(status))
			throw ConfigError(name.line, "duplicate error_page for status " + args[0]);
		server.error_page[status] = args[1];
		return;
	}
	if (!seen.insert(name.text).second)
		throw ConfigError(name.line, "duplicate " + name.text);
	expectArgs(name, args, 1);
	if (name.text == "root")
		server.root = args[0];
	else if (name.text == "index")
		server.index = args[0];
	else
		server.client_max_body_size = parseBodySize(args[0], name.line);
}

// C4b replaces this with real location parsing; for now only the shape is checked.
static void skipLocation(const std::vector<Token> &tokens, std::size_t &pos,
	const Token &name)
{
	if (pos >= tokens.size() || tokens[pos].type != Token::Word)
		throw ConfigError(name.line, "location expects a path");
	++pos;
	if (pos >= tokens.size() || tokens[pos].type != Token::OpenBrace)
		throw ConfigError(tokens[pos - 1].line, "expected '{' after location path");
	++pos;
	while (pos < tokens.size() && tokens[pos].type != Token::CloseBrace)
	{
		if (tokens[pos].type == Token::OpenBrace)
			throw ConfigError(tokens[pos].line, "unexpected '{'");
		++pos;
	}
	if (pos >= tokens.size())
		throw ConfigError(lastLine(tokens), "missing '}'");
	++pos;
}

// Called after "server {"; consumes everything up to the matching '}'.
static ServerConfig parseServer(const std::vector<Token> &tokens, std::size_t &pos,
	int serverLine)
{
	ServerConfig server;
	std::set<std::string> seen;
	while (true)
	{
		if (pos >= tokens.size())
			throw ConfigError(lastLine(tokens), "missing '}'");
		const Token &name = tokens[pos++];
		if (name.type == Token::CloseBrace)
			break;
		if (name.type != Token::Word)
			throw ConfigError(name.line, "unexpected '" + name.text + "'");
		if (name.text == "location")
		{
			skipLocation(tokens, pos, name);
			continue;
		}
		if (isLocationDirective(name.text))
			throw ConfigError(name.line, name.text + " is not allowed in a server block");
		if (!isServerDirective(name.text))
			throw ConfigError(name.line, "unknown directive '" + name.text + "'");
		applyServerDirective(server, seen, name, readArgs(tokens, pos, name));
	}
	if (server.listen.empty())
		throw ConfigError(serverLine, "server block has no listen directive");
	return server;
}

std::vector<ServerConfig> parseConfig(const std::vector<Token> &tokens)
{
	std::vector<ServerConfig> servers;
	std::size_t pos = 0;
	while (pos < tokens.size())
	{
		const Token &word = tokens[pos++];
		if (word.type != Token::Word || word.text != "server")
			throw ConfigError(word.line, "expected 'server', found '" + word.text + "'");
		if (pos >= tokens.size() || tokens[pos].type != Token::OpenBrace)
			throw ConfigError(word.line, "expected '{' after server");
		++pos;
		servers.push_back(parseServer(tokens, pos, word.line));
	}
	if (servers.empty())
		throw ConfigError(lastLine(tokens), "no server block");
	return servers;
}
