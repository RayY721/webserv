#include <iostream>
#include <string>
#include "ServerConfig.hpp"
#include "Listener.hpp"
#include <vector>
#include <map>
#include <unistd.h>

int main(int argc, char **argv)
{
	if (argc > 2)
	{
		std::cerr << "Usage: ./webserv [configuration file]" << std::endl;
		return 1;
	}

	std::string configPath = "conf/default.conf";
	if (argc == 2)
		configPath = argv[1];

	std::cout << "webserv: using " << configPath << std::endl;

	// TODO: C4a: replace with the parsed config. Hard-coded for A3.
	ServerConfig s1;
	s1.listen.emplace_back("127.0.0.1", 8080);
	ServerConfig s2;
	s2.listen.emplace_back("127.0.0.1", 8081);

	std::vector<ServerConfig> servers;
	servers.push_back(s1);
	servers.push_back(s2);

	std::map<int, const ServerConfig*> listeners;
	try
	{
		listeners = openListeners(servers);
	}
	catch (const std::exception &e)
	{
		std::cerr << "webserv: " << e.what() << std::endl;
		return 1;
	}

	for (const auto &entry : listeners)
		std::cout << "listening fd " << entry.first << " on port "
			<< entry.second->listen[0].port << std::endl;

	std::cout << "Press Enter to quit" << std::endl;
	// TODO: A4: replace with the poll() loop. Temporary: keeps the listeners open for testing.
	std::cin.get();

	for (const auto &entry : listeners)
		close(entry.first);

	return 0;
}
