#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <vector>
#include <set>

class Channel {
private:
    std::string _name;                  // チャネル名
    std::string _topic;                // チャネルのトピック <-- 追加
    std::vector<int> _client_fds;
    std::set<int> _operators;
    std::set<char> _modes;
    std::set<int> _invitees;
    std::string _password;  // チャンネルのパスワード

public:
    Channel();
    Channel(const std::string &name);
    ~Channel();

    const std::string& getName() const;

    // トピックのGetter/Setterを追加
    const std::string& getTopic() const;
    void setTopic(const std::string &topic);

    void addClient(int client_fd);
    void removeClient(int client_fd);
    const std::vector<int>& getClients() const;
    void addOperator(int client_fd);
    void removeOperator(int client_fd);
    bool isOperator(int client_fd) const;
    void addMode(char mode);
    void removeMode(char mode);
    bool hasMode(char mode) const;
    void addInvitee(int client_fd);
    bool isInvitee(int client_fd) const;

    // この関数の実装が必要！
    void setPassword(const std::string &password);
    const std::string& getPassword() const;
    bool checkPassword(const std::string &password) const;
};

#endif // CHANNEL_HPP