#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>
#include <map>
#include <sys/socket.h>
#include <netinet/in.h>
#include "channel.hpp"

// クライアント情報を管理する構造体
struct ClientInfo {
    std::string nickname;
    std::string username;
};

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
    void handleClient(int client_fd); // クライアントからのデータを処理する
    void removeClient(int client_fd); // クライアント接続を切断し管理から削除
    bool authenticateClient(int client_fd); // クライアントの認証を行う
    void handleNickCommand(int client_fd, const std::string &nickname); // ニックネーム設定コマンドを処理する
    void handleUserCommand(int client_fd, const std::string &username); // ユーザー名設定コマンドを処理する
    void handleJoinCommand(int client_fd, const std::string &channel_name); // チャネル参加コマンドを処理する
    void handlePrivmsgCommand(int client_fd, const std::string &target, const std::string &message); // メッセージ送信コマンドを処理する
    void createChannel(const std::string &channel_name); // 新しいチャネルを作成する
    void joinChannel(int client_fd, const std::string &channel_name); // クライアントをチャネルに参加させる

public:
    // コンストラクタとデストラクタ
    Server(int port, const std::string &password);
    ~Server();

    // サーバーを起動する
    void start();

    // サーバーのシャットダウン処理
    void shutdown();

    // エラーメッセージを表示
    void logError(const std::string &message);
};

#endif // SERVER_HPP