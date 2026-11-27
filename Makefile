NAME = webserv
DUMP = config_dump
REQUEST_DUMP = request_dump                                    # NEW

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++17 -Iinclude

CONFIG_SRCS = src/config/ConfigLexer.cpp
NETWORK_SRCS = src/network/Listener.cpp
HTTP_SRCS = src/http/RequestParser.cpp

SRCS = main.cpp $(CONFIG_SRCS) $(NETWORK_SRCS) $(HTTP_SRCS)
OBJS = $(SRCS:.cpp=.o)

DUMP_SRCS = tests/config_dump.cpp $(CONFIG_SRCS)
DUMP_OBJS = $(DUMP_SRCS:.cpp=.o)

REQUEST_DUMP_SRCS = tests/request_dump.cpp $(HTTP_SRCS)       # NEW
REQUEST_DUMP_OBJS = $(REQUEST_DUMP_SRCS:.cpp=.o)               # NEW

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

$(DUMP): $(DUMP_OBJS)
	$(CXX) $(CXXFLAGS) $(DUMP_OBJS) -o $(DUMP)

$(REQUEST_DUMP): $(REQUEST_DUMP_OBJS)                          # NEW
	$(CXX) $(CXXFLAGS) $(REQUEST_DUMP_OBJS) -o $(REQUEST_DUMP)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(DUMP_OBJS) $(REQUEST_DUMP_OBJS)

fclean: clean
	rm -f $(NAME) $(DUMP) $(REQUEST_DUMP)

re: fclean all

.PHONY: all clean fclean re
