#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <vector>
#include <set>

class Channel {
private:
    std::string _name;                  // チャネル名
    std::vector<int> _client_fds;       // チャネルに参加しているクライアントのファイルディスクリプタ
    std::set<int> _operators;           // チャネルのオペレーターのファイルディスクリプタ
    std::set<char> _modes;              // チャネルのモード
    std::set<int> _invitees;            // 招待されたクライアントのファイルディスクリプタ

public:
    Channel();
    Channel(const std::string &name);
    ~Channel();

    const std::string& getName() const;
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
};

#endif // CHANNEL_HPP