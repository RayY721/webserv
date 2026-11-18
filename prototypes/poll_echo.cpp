// getaddrinfo(), freeaddrinfo(), socket(), listen()
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
// fcntl();
#include <fcntl.h>

#include <unistd.h> // close(), pause()

// poll()
#include <poll.h>

#include <iostream>
#include <cstring> // strerror
#include <cerrno> // errno

#include <vector>
#include <map>

// EXIT_FAILURE, EXIT_SUCCESS
#include <cstdlib>

void removeClient(std::vector<pollfd> &fds, std::map<int, std::string> &buffers, size_t i)
{
	int fd = fds[i].fd;
	close(fd); // free up fd number in kernel
	fds.erase(fds.begin() + i); // remove pollfd in fds, else poll keeps watching a closed fd
	buffers.erase(fd); // the next client to connect gets the same fd back. without erase it would start with the prev client's leftover bytes in the buffer
}

int main()
{
	// ** Set up server **
	// Describe the address: hints, getaddrinfo
	struct addrinfo hints = {}; // zerorise
	hints.ai_family = AF_INET; // IPv4
	hints.ai_socktype = SOCK_STREAM; // TCP
	hints.ai_flags = AI_PASSIVE; // address is for bind(), not connect()

	struct addrinfo *res;
	int rc = getaddrinfo(NULL, "8080", &hints, &res);
	if (rc != 0) // returns an error code, does not set errno
	{
		std::cerr << "getaddrinfo: " << gai_strerror(rc) << std::endl; // gai_strerror turns getaddrinfo's error codes into text
		return EXIT_FAILURE;
	}

	// Create and configure the socket: socket, setsockopt, fcntl, bind
	int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol); // how is ai_protocol set? never ourselves. zerorised at the start - so it means any protocol that fits. getaddrinfo see SOCK_STREAM + AFINET so ai_protocol becomes IPPROTO_TCP, which is 6.
	if (sockfd == -1)
	{
		std::cerr << "socket: " << std::strerror(errno) << std::endl;
		freeaddrinfo(res); // memory allocated by successful call to getaddinfo() has to be freed
		return EXIT_FAILURE;
	}
	std::cout << "sockfd is: " << sockfd << std::endl;

	int yes = 1; // setsockopt wants a pointer to the value.
	// SO_REUSEADDR allows server to restart straight away after a crash or ctrl-c. port 8080 will get stuck in TIME_WAIT state for a while, and bind fails till timer runs out
	// O_NONBLOCK is set to make accept / recv /  send to return -1 immediately instead of freezing if they cannot do anything right away, making our server non-blocking
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1
		|| fcntl(sockfd, F_SETFL, O_NONBLOCK) == -1
		|| bind(sockfd, res->ai_addr, res->ai_addrlen) == -1)
	{
		std::cerr << "setup: " << std::strerror(errno) << std::endl;
		freeaddrinfo(res);
		close(sockfd);
		return EXIT_FAILURE;
	}

	// free res, start listening
	freeaddrinfo(res);

	if (listen(sockfd, SOMAXCONN) == -1)
	{
		std::cerr << "listen: " << std::strerror(errno) << std::endl;
		close(sockfd);
		return EXIT_FAILURE;
	}

	std::cout << "listening on 8080" << std::endl;

	std::vector<pollfd> fds; // create an empty vector.
	fds.push_back({sockfd, POLLIN, 0}); // fd, events, revents. fds[0] is pollfd, whose .fd is sockfd

	std::map<int, std::string> buffers{}; // map container used to store the key (fd) and value (string). fd → bytes received from that client but not yet sent back

	// Poll loop
	while (true)
	{
		// poll (sleep until something is ready).
		int ready = poll(fds.data(), fds.size(), -1); // -1 as the timeout means the poll blocks indefinitely - never times out and only returns when at least one fd is ready or on an error. 0 cannot happen
		if (ready == -1)
		{
			std::cerr << "poll: " << std::strerror(errno) << std::endl;
			return EXIT_FAILURE;
		}

		// if listener (fds[0]) is ready, accept the new client (creating a new fd) and add it to the watch list (fds)
		if (fds[0].revents & POLLIN)
		{
			int new_client_fd = accept(sockfd, nullptr, nullptr); // dont need the client's ip, so pass nullptr
			if (new_client_fd == -1)
			{
				std::cerr << "accept: " << std::strerror(errno) << std::endl;
				continue; // drop only this connection, server stays up
			}
			if (fcntl(new_client_fd, F_SETFL, O_NONBLOCK) == -1) // make the client socket non-blocking, like the listener.
			{
				std::cerr << "fcntl: " << std::strerror(errno) << std::endl;
				close(new_client_fd);
				continue; // drop only this connection, server stays up
			}
			fds.push_back({new_client_fd, POLLIN, 0});
			std::cout << "new client fd: " << new_client_fd << std::endl;
		}

		// for each client: check if it is hung up (then clean up). readable (bytes arrived, so recv). writeable (room to send, so send)
		size_t i = 1; // start indexing at 1 as 0 is the listening socket
		while (i < fds.size())
		{
			int fd = fds[i].fd;
			std::string &buf = buffers[fd]; // buffers[fd] creates n empty string the first time it sees a new fd. reference is safe because map entries never move

			// Handle device / socket disconnection or exceptional condition occuring on device or socket
			if (fds[i].revents & POLLHUP || fds[i].revents & POLLERR)
			{
				removeClient(fds, buffers, i);
				continue;
			}

			// data ready to read (client has sent bytes) or client has closed
			if (fds[i].revents & POLLIN)
			{
				char tmp[4096]; // size can be changed
				ssize_t n = recv(fds[i].fd, tmp, sizeof(tmp), 0);
				if (n <= 0) // 0 = client closed the connection, -1 = error: either way, drop the client
				{
					removeClient(fds, buffers, i);
					continue;
				}
				buf.append(tmp, n);
				fds[i].events = POLLOUT; // no longer waiting for data from this client, wait until we can send it back
			}

			// socket is writable (room in the send buffer), so send back what we have
			else if (fds[i].revents & POLLOUT)
			{
				ssize_t sent = send(fds[i].fd, buf.data(), buf.length(), 0);
				if (sent > 0) // send may take only part of the buffer. erase removes what was sent and the rest goes out on the next POLLOUT
				{
					buf.erase(0, sent);
					if (buf.length() == 0)
						fds[i].events = POLLIN; // now buffer is empty so back to waiting for data from this client
				}
			}
			++i; // onlyy reached if the client was not removed. After removeClient, the next client has moved into slot i.
		}
	}

	close(sockfd);

	return EXIT_SUCCESS;
}
