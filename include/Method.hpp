#ifndef METHOD_HPP
#define METHOD_HPP

// The methods webserv handles. There are only a few, so each one has its own value.
// Any other method (PUT, FOO...) becomes Unknown, and B5 rejects it.
// Shared by Request (B1) and LocationConfig (C2), so both use the same type.
enum class Method
{
	Get,
	Post,
	Delete,
	Unknown
};

#endif
