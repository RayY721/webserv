#include "Listener.hpp"
#include <string>

// getaddrinfo(), freeaddrinfo(), socket(), listen()
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>


#include <fcntl.h>
#include <cstring> // strerror
#include <cerrno>
#include <unistd.h> // close()
#include <stdexcept>

static int openListener(const ListenAddress &addr)
{
	std::string port = std::to_string(addr.port);

	struct addrinfo hints = {};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	struct addrinfo *res;
	std::string err_prefix = addr.address + ":" + port + ": ";

	int rc = getaddrinfo(addr.address.c_str(), port.c_str(), &hints, &res);
	if (rc != 0) // returns an error code, does not set errno
		throw std::runtime_error(err_prefix + "getaddrinfo: " + gai_strerror(rc));

	int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (sockfd == -1)
	{
		std::string err = err_prefix + "socket: " + std::strerror(errno);
		freeaddrinfo(res);
		throw std::runtime_error(err);
	}

	int yes = 1;
	// Lets a restarted server rebind at once instead of failing while the port is in TIME_WAIT.
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1)
	{
		std::string err = err_prefix + "setsockopt: " + std::strerror(errno);
		freeaddrinfo(res);
		close(sockfd);
		throw std::runtime_error(err);
	}

	// accept() must never block the poll() loop.
	if (fcntl(sockfd, F_SETFL, O_NONBLOCK) == -1)
	{
		std::string err = err_prefix + "fcntl: " + std::strerror(errno);
		freeaddrinfo(res);
		close(sockfd);
		throw std::runtime_error(err);
	}

	if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1)
	{
		std::string err = err_prefix + "bind: " + std::strerror(errno);
		freeaddrinfo(res);
		close(sockfd);
		throw std::runtime_error(err);
	}

	freeaddrinfo(res);

	if (listen(sockfd, SOMAXCONN) == -1)
	{
		std::string err = err_prefix + "listen: " + std::strerror(errno);
		close(sockfd);
		throw std::runtime_error(err);
	}

	return sockfd;
}

std::map<int, const ServerConfig*> openListeners(const std::vector<ServerConfig> &servers)
{
	std::map<int, const ServerConfig*> listeners;
	try
	{
		for (const ServerConfig &server : servers)
		{
			for (const ListenAddress &addr : server.listen)
			{
				int fd = openListener(addr);
				listeners[fd] = &server;
			}
		}
	}
	catch(...) // catches anything so fds get closed with whatever that is thrown
	{
		for (const auto &entry : listeners)
			close(entry.first);
		throw;
	}

	return listeners;

}