
#include <iostream>
#include <string>

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
	return 0;
}
