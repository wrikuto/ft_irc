#include "../include/channel.hpp"
#include <algorithm>

// コンストラクタでトピックを初期化
Channel::Channel() : _name(""), _topic("") {}

Channel::Channel(const std::string &name)
    : _name(name), _topic("") {}

Channel::~Channel() {}

const std::string& Channel::getName() const {
    return _name;
}

// 追加したメソッドの定義
const std::string& Channel::getTopic() const {
    return _topic;
}

void Channel::setTopic(const std::string &topic) {
    _topic = topic;
}

void Channel::addClient(int client_fd) {
    _client_fds.push_back(client_fd);
    _invitees.erase(client_fd);
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


void Channel::setPassword(const std::string &password) {
    _password = password;
}


const std::string& Channel::getPassword() const {
    return _password;
}


bool Channel::checkPassword(const std::string &password) const {
    // パスワードが設定されていない場合は常にtrue
    if (_password.empty()) {
        return true;
    }
    // パスワードが一致するかチェック
    return password == _password;
}