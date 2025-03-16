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
    bool authenticated;  // 認証完了したかどうかのフラグ
    bool password_sent;  // パスワードプロンプトを送信済みかのフラグ
    
    ClientInfo() : authenticated(false), password_sent(false) {}
};

/**
 * @brief サーバークラス。
 * 
 * クライアントからの接続を受け入れ、メッセージの送受信を処理するサーバークラス。
 */
class Server {
private:
    int _port;                              // サーバーがリスニングするポート番号
    std::string _password;                  // 接続時に必要なパスワード
    int _server_fd;                         // サーバーソケットのファイルディスクリプタ
    std::vector<int> _client_fds;           // 接続中のクライアントソケットのファイルディスクリプタ
    fd_set _read_fds;                       // `select` 用の読み取りセット
    fd_set _write_fds;                      // 書き込みセット（必要に応じて使用）
    std::map<int, ClientInfo> _clients;     // クライアント情報を管理するデータ構造
    std::map<std::string, Channel> _channels;    // チャネルを管理するデータ構造
    std::map<int, std::string> _clientBuffers;   // 各クライアントで受信途中のデータを保持

    // 内部メソッド
    void acceptClient();                       // 新しいクライアント接続を受け入れる
    void handleClient(int client_fd);          // クライアントからのデータを処理
    void removeClient(int client_fd);          // クライアント接続を切断し管理から削除
    bool authenticateClient(int client_fd);    // クライアントの認証を行う

    // IRCコマンドハンドラ
    void handleNickCommand(int client_fd, const std::string &nickname); 
    void handleUserCommand(int client_fd, const std::string &username);
    void handleJoinCommand(int client_fd, const std::string &channel_name, const std::string &password);
    void handlePrivmsgCommand(int client_fd, const std::string &target, const std::string &message);
    void handleKickCommand(int client_fd, const std::string &channel_name, const std::string &target_nickname);
    void handleModeCommand(int client_fd, const std::string &channel_name, const std::string &mode, const std::string &parameter);
    void handleInviteCommand(int client_fd, const std::string &target_nickname, const std::string &channel_name);
    void handleTopicCommand(int client_fd, const std::string &channel_name, const std::string &new_topic);

    // チャネル関連メソッド
    void createChannel(const std::string &channel_name, int client_fd);  
    void joinChannel(int client_fd, const std::string &channel_name);    
    void inviteUser(int client_fd, const std::string &channel_name, const std::string &target_nickname);

public:
    Server(int port, const std::string &password);
    ~Server();

    void start();                      // サーバーを起動してメインループに入る
    void shutdown();                   // サーバーを停止する（未実装の場合は将来拡張用）
    void logError(const std::string &message); // エラーログを出力する（未実装の場合は将来拡張用）
};

#endif // SERVER_HPP