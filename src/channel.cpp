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
    _invitees.erase(client_fd); // 参加後は招待リストから削除
}

void Channel::removeClient(int client_fd) {
    _client_fds.erase(std::remove(_client_fds.begin(), _client_fds.end(), client_fd), _client_fds.end());
}

const std::vector<int>& Channel::getClients() const {
    return _client_fds;
}

void Channel::addOperator(int client_fd) {
    _operators.insert(client_fd);
}

void Channel::removeOperator(int client_fd) {
    _operators.erase(client_fd);
}

bool Channel::isOperator(int client_fd) const {
    return _operators.find(client_fd) != _operators.end();
}

void Channel::addMode(char mode) {
    _modes.insert(mode);
}

void Channel::removeMode(char mode) {
    _modes.erase(mode);
}

bool Channel::hasMode(char mode) const {
    return _modes.find(mode) != _modes.end();
}

void Channel::addInvitee(int client_fd) {
    _invitees.insert(client_fd);
}

bool Channel::isInvitee(int client_fd) const {
    return _invitees.find(client_fd) != _invitees.end();
}