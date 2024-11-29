NAME = ircserv
CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98
SRCS = $(wildcard src/*.cpp src/commands/*.cpp)
OBJS = $(SRCS:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJS)

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all
