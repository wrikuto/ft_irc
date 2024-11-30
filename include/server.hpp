#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>
#include <map>
#include <sys/socket.h>
#include <netinet/in.h>
#include "channel.hpp"



/**
 * @brief クライアントの情報を保持する構造体。
 * 
 * クライアントのニックネームとユーザー名を保持する構造体。
 */
struct ClientInfo {
    std::string nickname;
    std::string username;
};


/**
 * @brief サーバークラス。
 * 
 * クライアントからの接続を受け入れ、メッセージの送受信を処理するサーバークラス。
 */
class Server {
private:
    int _port;                     // サーバーがリスニングするポート番号
    std::string _password;         // 接続時に必要なパスワード
    int _server_fd;                // サーバーソケットのファイルディスクリプタ
    std::vector<int> _client_fds;  // 接続中のクライアントソケットのファイルディスクリプタ
    fd_set _read_fds;              // `select` または `poll` 用の読み取りセット
    fd_set _write_fds;             // 書き込みセット（必要に応じて使用）
    std::map<int, ClientInfo> _clients; // クライアント情報を管理するデータ構造
    std::map<std::string, Channel> _channels; // チャネルを管理するデータ構造

    void acceptClient();           // 新しいクライアント接続を受け入れる
    void handleClient(int client_fd); // クライアントからのデータを処理
    void removeClient(int client_fd); // クライアント接続を切断し管理から削除
    bool authenticateClient(int client_fd); // クライアントの認証を行う

    // コマンドを処理するメソッド
    void handleNickCommand(int client_fd, const std::string &nickname); 
    void handleUserCommand(int client_fd, const std::string &username);
    void handleJoinCommand(int client_fd, const std::string &channel_name);
    void handlePrivmsgCommand(int client_fd, const std::string &target, const std::string &message);
    void handleKickCommand(int client_fd, const std::string &channel_name, const std::string &target_nickname);
    void handleModeCommand(int client_fd, const std::string &channel_name, const std::string &mode);
    void handleInviteCommand(int client_fd, const std::string &target_nickname, const std::string &channel_name);

    void createChannel(const std::string &channel_name, int client_fd); // 新しいチャネルを作成
    void joinChannel(int client_fd, const std::string &channel_name); // クライアントをチャネルに参加させる
    void inviteUser(int client_fd, const std::string &channel_name, const std::string &target_nickname); // ユーザーをチャネルに招待

public:
    Server(int port, const std::string &password);
    ~Server();

    void start();
    void shutdown();
    void logError(const std::string &message);
};

#endif // SERVER_HPP