#include "EventLoop.hpp"
#include "Client.hpp"
#include <poll.h>
#include <csignal>
#include <vector>
#include <iostream>
#include <sys/socket.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <exception>

void runEventLoop(const std::map<int, const ServerConfig*> &listeners)
{
	signal(SIGPIPE, SIG_IGN);

	std::map<int, Client> clients;

	while(true)
	{
		std::vector<pollfd> fds;
		try
		{
			for (const auto &entry : listeners)
				fds.push_back({entry.first, POLLIN, 0});
			for (const auto &entry : clients)
				fds.push_back({entry.first, entry.second.pollEvents(), 0});
		}
		catch (const std::exception &e)
		{
			std::cerr << "failed to build fds vector: " << e.what() << std::endl;
			continue;
		}

		int ready = poll(fds.data(), fds.size(), -1); // No timeout until A6 (idle clients).
		if (ready == -1)
		{
			std::cerr << "poll: " << std::strerror(errno) << std::endl;
			continue;
		}
		for (size_t i = 0; i < fds.size(); ++i)
		{
			if (fds[i].revents == 0)
				continue;

			auto lit = listeners.find(fds[i].fd);
			if (lit != listeners.end())
			{
				if (fds[i].revents & POLLIN)
				{
					int clientFd = accept(lit->first, nullptr, nullptr);
					if (clientFd == -1)
						continue;
					if (fcntl(clientFd, F_SETFL, O_NONBLOCK) == -1)
					{
						close(clientFd);
						continue;
					}
					try
					{
						clients.try_emplace(clientFd, clientFd, lit->second);
					}
					catch (...)
					{
						std::cerr << "accept: could not store the client, dropping it" << std::endl;
						close(clientFd);
					}
				}
				continue;
			}

			auto cit = clients.find(fds[i].fd);
			if (cit == clients.end())
				continue;
			Client &client = cit->second;
			try
			{
				if (fds[i].revents & POLLHUP || fds[i].revents & POLLERR)
				{
					clients.erase(cit);
					continue;
				}

				if (fds[i].revents & POLLIN)
				{
					char tmp[4096];
					ssize_t n = recv(fds[i].fd, tmp, sizeof(tmp), 0);
					if (n <= 0)
					{
						clients.erase(cit);
						continue;
					}
					client.readBuffer.append(tmp, n);
					client.touch();
				}
			}
			catch (const std::exception &e)
			{
				std::cerr << "client " << fds[i].fd << ": " << e.what() << std::endl;
				clients.erase(cit);
			}
			catch(...)
			{
				clients.erase(cit);
			}
		}
	}
}
