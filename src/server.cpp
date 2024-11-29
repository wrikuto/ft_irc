#include "../include/server.hpp"
#include "../include/channel.hpp"
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cerrno>
#include <sstream>
#include <algorithm> // std::removeを使用するために追加

Server::Server(int port, const std::string &password): _port(port), _password(password), _server_fd(-1) {}

Server::~Server() {
    if (_server_fd != -1) close(_server_fd);
}

void Server::start() {
    if ((_server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        std::cerr << "Socket creation failed: " << strerror(errno) << std::endl;
        return;
    }

    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(_port);

    if (bind(_server_fd, (struct sockaddr*)&address, sizeof(address)) == -1) {
        std::cerr << "Bind failed: " << strerror(errno) << std::endl;
        close(_server_fd);
        return;
    }

    if (listen(_server_fd, 10) == -1) {
        std::cerr << "Listen failed: " << strerror(errno) << std::endl;
        close(_server_fd);
        return;
    }

    std::cout << "Server started on port " << _port << std::endl;

    // 接続の受け入れロジックを追加
    while (true) {
        FD_ZERO(&_read_fds);
        FD_SET(_server_fd, &_read_fds);
        int max_fd = _server_fd;

        for (size_t i = 0; i < _client_fds.size(); ++i) {
            int fd = _client_fds[i];
            FD_SET(fd, &_read_fds);
            if (fd > max_fd) {
                max_fd = fd;
            }
        }

        int activity = select(max_fd + 1, &_read_fds, NULL, NULL, NULL);
        if (activity < 0 && errno != EINTR) {
            std::cerr << "Select error: " << strerror(errno) << std::endl;
        }

        if (FD_ISSET(_server_fd, &_read_fds)) {
            acceptClient();
        }

        for (size_t i = 0; i < _client_fds.size(); ++i) {
            int fd = _client_fds[i];
            if (FD_ISSET(fd, &_read_fds)) {
                handleClient(fd);
            }
        }
    }
}

void Server::acceptClient() {
    int client_fd;
    sockaddr_in client_address;
    socklen_t client_address_len = sizeof(client_address);

    if ((client_fd = accept(_server_fd, (struct sockaddr*)&client_address, &client_address_len)) == -1) {
        std::cerr << "Accept failed: " << strerror(errno) << std::endl;
        return;
    }

    // クライアントの認証
    if (!authenticateClient(client_fd)) {
        close(client_fd);
        return;
    }

    _client_fds.push_back(client_fd);
    _clients[client_fd] = ClientInfo(); // クライアント情報を初期化
    std::cout << "New client connected: " << client_fd << std::endl;
}

bool Server::authenticateClient(int client_fd) {
    char buffer[1024] = {0};
    std::string password_prompt = "Enter server password: ";
    send(client_fd, password_prompt.c_str(), password_prompt.size(), 0);

    int valread = read(client_fd, buffer, 1024);
    if (valread <= 0) {
        std::cerr << "Failed to read password from client: " << strerror(errno) << std::endl;
        return false;
    }

    std::string received_password(buffer, valread);
    received_password.erase(received_password.find_last_not_of(" \n\r\t") + 1); // トリム

    if (received_password != _password) {
        std::string error_message = "Incorrect password. Connection closed.\n";
        send(client_fd, error_message.c_str(), error_message.size(), 0);
        return false;
    }

    std::string success_message = "Password accepted. Welcome!\n";
    send(client_fd, success_message.c_str(), success_message.size(), 0);
    return true;
}

void Server::handleClient(int client_fd) {
    char buffer[1024] = {0};
    int valread = read(client_fd, buffer, 1024);
    if (valread <= 0) {
        std::cerr << "Failed to read from client: " << strerror(errno) << std::endl;
        removeClient(client_fd);
        return;
    }

    std::istringstream iss(std::string(buffer, valread));
    std::string command;
    iss >> command;

    if (command == "NICK") {
        std::string nickname;
        iss >> nickname;
        handleNickCommand(client_fd, nickname);
    } else if (command == "USER") {
        std::string username;
        iss >> username;
        handleUserCommand(client_fd, username);
    } else if (command == "JOIN") {
        std::string channel_name;
        iss >> channel_name;
        handleJoinCommand(client_fd, channel_name);
    } else if (command == "PRIVMSG") {
        std::string target;
        iss >> target;
        std::string message;
        std::getline(iss, message);
        handlePrivmsgCommand(client_fd, target, message);
    } else if (command == "KICK") {
        std::string channel_name;
        std::string target_nickname;
        iss >> channel_name >> target_nickname;
        handleKickCommand(client_fd, channel_name, target_nickname);
    } else if (command == "MODE") {
        std::string channel_name;
        std::string mode;
        iss >> channel_name >> mode;
        handleModeCommand(client_fd, channel_name, mode);
    } else if (command == "INVITE") {
        std::string target_nickname;
        std::string channel_name;
        iss >> target_nickname >> channel_name;
        handleInviteCommand(client_fd, target_nickname, channel_name);
    } else {
        std::string error_message = "Unknown command.\n";
        send(client_fd, error_message.c_str(), error_message.size(), 0);
    }
}

void Server::handleNickCommand(int client_fd, const std::string &nickname) {
    _clients[client_fd].nickname = nickname;
    std::string response = "Nickname set to " + nickname + "\n";
    send(client_fd, response.c_str(), response.size(), 0);
}

void Server::handleUserCommand(int client_fd, const std::string &username) {
    _clients[client_fd].username = username;
    std::string response = "Username set to " + username + "\n";
    send(client_fd, response.c_str(), response.size(), 0);
}

void Server::handleJoinCommand(int client_fd, const std::string &channel_name) {
    if (_channels.find(channel_name) == _channels.end()) {
        createChannel(channel_name, client_fd);
    } else if (_channels[channel_name].hasMode('i') && !_channels[channel_name].isInvitee(client_fd)) {
        std::string error_message = "Cannot join channel (+i)\n";
        send(client_fd, error_message.c_str(), error_message.size(), 0);
        return;
    }
    joinChannel(client_fd, channel_name);
}


void Server::handleInviteCommand(int client_fd, const std::string &target_nickname, const std::string &channel_name) {
    if (_channels.find(channel_name) == _channels.end()) {
        std::string error_message = "No such channel: " + channel_name + "\n";
        send(client_fd, error_message.c_str(), error_message.size(), 0);
        return;
    }

    if (!_channels[channel_name].isOperator(client_fd)) {
        std::string error_message = "You are not an operator of channel: " + channel_name + "\n";
        send(client_fd, error_message.c_str(), error_message.size(), 0);
        return;
    }

    int target_fd = -1;
    for (std::map<int, ClientInfo>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        if (it->second.nickname == target_nickname) {
            target_fd = it->first;
            break;
        }
    }

    if (target_fd == -1) {
        std::string error_message = "No such user: " + target_nickname + "\n";
        send(client_fd, error_message.c_str(), error_message.size(), 0);
        return;
    }

    _channels[channel_name].addInvitee(target_fd);
    std::string response = "User " + target_nickname + " has been invited to channel " + channel_name + "\n";
    send(client_fd, response.c_str(), response.size(), 0);
    send(target_fd, response.c_str(), response.size(), 0);
}


void Server::handlePrivmsgCommand(int client_fd, const std::string &target, const std::string &message) {
    if (_channels.find(target) != _channels.end()) {
        // チャネルへのメッセージ
        const std::vector<int>& clients = _channels[target].getClients();
        if (_channels[target].hasMode('n') && std::find(clients.begin(), clients.end(), client_fd) == clients.end()) {
            std::string error_message = "You are not in channel: " + target + "\n";
            send(client_fd, error_message.c_str(), error_message.size(), 0);
            return;
        }
        if (_channels[target].hasMode('m') && !_channels[target].isOperator(client_fd)) {
            std::string error_message = "Channel is moderated. Only operators can send messages.\n";
            send(client_fd, error_message.c_str(), error_message.size(), 0);
            return;
        }
        for (size_t i = 0; i < clients.size(); ++i) {
            if (clients[i] != client_fd) {
                std::string full_message = _clients[client_fd].nickname + ": " + message + "\n";
                send(clients[i], full_message.c_str(), full_message.size(), 0);
            }
        }
    } else {
        // 個人へのメッセージ
        int target_fd = -1;
        for (std::map<int, ClientInfo>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
            if (it->second.nickname == target) {
                target_fd = it->first;
                break;
            }
        }

        if (target_fd == -1) {
            std::string error_message = "No such user: " + target + "\n";
            send(client_fd, error_message.c_str(), error_message.size(), 0);
            return;
        }

        std::string full_message = _clients[client_fd].nickname + ": " + message + "\n";
        send(target_fd, full_message.c_str(), full_message.size(), 0);
    }
}

void Server::createChannel(const std::string &channel_name, int client_fd) {
    _channels[channel_name] = Channel(channel_name);
    _channels[channel_name].addOperator(client_fd); // チャネル作成者をオペレーターとして設定
    std::cout << "Channel created: " << channel_name << std::endl;
}

void Server::joinChannel(int client_fd, const std::string &channel_name) {
    _channels[channel_name].addClient(client_fd);
    std::string response = "Joined channel " + channel_name + "\n";
    send(client_fd, response.c_str(), response.size(), 0);
}

void Server::handleKickCommand(int client_fd, const std::string &channel_name, const std::string &target_nickname) {
    if (_channels.find(channel_name) == _channels.end()) {
        std::string error_message = "No such channel: " + channel_name + "\n";
        send(client_fd, error_message.c_str(), error_message.size(), 0);
        return;
    }

    if (!_channels[channel_name].isOperator(client_fd)) {
        std::string error_message = "You are not an operator of channel: " + channel_name + "\n";
        send(client_fd, error_message.c_str(), error_message.size(), 0);
        return;
    }

    int target_fd = -1;
    for (std::map<int, ClientInfo>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        if (it->second.nickname == target_nickname) {
            target_fd = it->first;
            break;
        }
    }

    if (target_fd == -1) {
        std::string error_message = "No such user: " + target_nickname + "\n";
        send(client_fd, error_message.c_str(), error_message.size(), 0);
        return;
    }

    _channels[channel_name].removeClient(target_fd);
    std::string response = "User " + target_nickname + " has been kicked from channel " + channel_name + "\n";
    send(client_fd, response.c_str(), response.size(), 0);
    send(target_fd, response.c_str(), response.size(), 0);
}

void Server::handleModeCommand(int client_fd, const std::string &channel_name, const std::string &mode) {
    if (_channels.find(channel_name) == _channels.end()) {
        std::string error_message = "No such channel: " + channel_name + "\n";
        send(client_fd, error_message.c_str(), error_message.size(), 0);
        return;
    }

    if (!_channels[channel_name].isOperator(client_fd)) {
        std::string error_message = "You are not an operator of channel: " + channel_name + "\n";
        send(client_fd, error_message.c_str(), error_message.size(), 0);
        return;
    }

    if (mode[0] == '+') {
        _channels[channel_name].addMode(mode[1]);
    } else if (mode[0] == '-') {
        _channels[channel_name].removeMode(mode[1]);
    }

    std::string response = "Channel mode for " + channel_name + " has been changed to " + mode + "\n";
    send(client_fd, response.c_str(), response.size(), 0);
}

void Server::removeClient(int client_fd) {
    close(client_fd);
    _client_fds.erase(std::remove(_client_fds.begin(), _client_fds.end(), client_fd), _client_fds.end());
    _clients.erase(client_fd);
    std::cout << "Client disconnected: " << client_fd << std::endl;
}

void Server::inviteUser(int client_fd, const std::string &channel_name, const std::string &target_nickname) {
    if (_channels.find(channel_name) == _channels.end()) {
        std::string error_message = "No such channel: " + channel_name + "\n";
        send(client_fd, error_message.c_str(), error_message.size(), 0);
        return;
    }

    if (!_channels[channel_name].isOperator(client_fd)) {
        std::string error_message = "You are not an operator of channel: " + channel_name + "\n";
        send(client_fd, error_message.c_str(), error_message.size(), 0);
        return;
    }

    int target_fd = -1;
    for (std::map<int, ClientInfo>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        if (it->second.nickname == target_nickname) {
            target_fd = it->first;
            break;
        }
    }

    if (target_fd == -1) {
        std::string error_message = "No such user: " + target_nickname + "\n";
        send(client_fd, error_message.c_str(), error_message.size(), 0);
        return;
    }

    _channels[channel_name].addInvitee(target_fd);
    std::string response = "User " + target_nickname + " has been invited to channel " + channel_name + "\n";
    send(client_fd, response.c_str(), response.size(), 0);
    send(target_fd, response.c_str(), response.size(), 0);
}