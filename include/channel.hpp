#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <vector>

class Channel {
private:
    std::string _name;                  // チャネル名
    std::vector<int> _client_fds;       // チャネルに参加しているクライアントのファイルディスクリプタ

public:
    Channel();
    Channel(const std::string &name);
    ~Channel();

    const std::string& getName() const;
    void addClient(int client_fd);
    void removeClient(int client_fd);
    const std::vector<int>& getClients() const;
};

#endif // CHANNEL_HPP