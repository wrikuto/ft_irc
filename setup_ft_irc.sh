#!/bin/bash

# ディレクトリ構造の定義
DIRS=(
    "include"
    "src"
    "src/commands"
    "config"
    "tests"
    "logs"
)

# 必要なファイル
FILES=(
    "include/server.hpp"
    "include/client.hpp"
    "include/channel.hpp"
    "include/utils.hpp"
    "include/commands.hpp"
    "src/main.cpp"
    "src/server.cpp"
    "src/client.cpp"
    "src/channel.cpp"
    "src/utils.cpp"
    "src/commands/nick.cpp"
    "src/commands/user.cpp"
    "src/commands/join.cpp"
    "src/commands/privmsg.cpp"
    "src/commands/kick.cpp"
    "src/commands/invite.cpp"
    "src/commands/topic.cpp"
    "src/commands/mode.cpp"
    "config/irc.conf"
    "tests/test_server.cpp"
    "tests/test_client.cpp"
    "tests/test_channel.cpp"
    "Makefile"
    "README.md"
)

# ディレクトリを作成
echo "Creating directories..."
for dir in "${DIRS[@]}"; do
    mkdir -p "$dir"
    echo "Created: $dir"
done

# 必要なファイルを作成
echo "Creating files..."
for file in "${FILES[@]}"; do
    touch "$file"
    echo "Created: $file"
done

# README.md のテンプレート
cat <<EOL > "README.md"
# ft_irc

This project implements an IRC server in C++98.

## Structure
- **include/**: Header files.
- **src/**: Source files, with commands located in src/commands.
- **config/**: Optional configuration files.
- **tests/**: Test programs.
- **logs/**: Runtime logs (created during execution).
- **Makefile**: Build system.
EOL
echo "Initialized README.md"

# Makefile のテンプレート
cat <<EOL > "Makefile"
NAME = ircserv
CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98
SRCS = \$(wildcard src/*.cpp src/commands/*.cpp)
OBJS = \$(SRCS:.cpp=.o)

all: \$(NAME)

\$(NAME): \$(OBJS)
	\$(CXX) \$(CXXFLAGS) -o \$(NAME) \$(OBJS)

clean:
	rm -f \$(OBJS)

fclean: clean
	rm -f \$(NAME)

re: fclean all
EOL
echo "Initialized Makefile"

echo "Setup completed. Your files and directories are ready!"
