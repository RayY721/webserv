NAME = webserv
DUMP = config_dump

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++17 -Iinclude

CONFIG_SRCS = src/config/ConfigLexer.cpp src/config/ConfigParser.cpp
NETWORK_SRCS = src/network/Listener.cpp
HTTP_SRCS = src/http/RequestParser.cpp

SRCS = main.cpp $(CONFIG_SRCS) $(NETWORK_SRCS) $(HTTP_SRCS)
OBJS = $(SRCS:.cpp=.o)

DUMP_SRCS = tests/config_dump.cpp $(CONFIG_SRCS)
DUMP_OBJS = $(DUMP_SRCS:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

$(DUMP): $(DUMP_OBJS)
	$(CXX) $(CXXFLAGS) $(DUMP_OBJS) -o $(DUMP)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(DUMP_OBJS)

fclean: clean
	rm -f $(NAME) $(DUMP)

re: fclean all

.PHONY: all clean fclean re
