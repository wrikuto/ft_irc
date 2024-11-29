#include "../include/channel.hpp"
#include <algorithm>

Channel::Channel() : _name("") {}

Channel::Channel(const std::string &name): _name(name) {}

Channel::~Channel() {}

const std::string& Channel::getName() const {
    return _name;
}

void Channel::addClient(int client_fd) {
    _client_fds.push_back(client_fd);
}

void Channel::removeClient(int client_fd) {
    _client_fds.erase(std::remove(_client_fds.begin(), _client_fds.end(), client_fd), _client_fds.end());
}

const std::vector<int>& Channel::getClients() const {
    return _client_fds;
}