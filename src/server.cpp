//// filepath: /home/wrikuto/1st_circle/ft_irc/src/server.cpp
#include "../include/server.hpp"
#include "../include/channel.hpp"
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cerrno>
#include <sstream>
#include <algorithm>
#include <fcntl.h>  // fcntlでのO_NONBLOCK設定に使用

/**
 * @brief コンストラクタ。サーバーポートとパスワードを設定し、ソケットFDの初期値を-1にする。
 */
Server::Server(int port, const std::string &password)
    : _port(port), _password(password), _server_fd(-1) {
}

/**
 * @brief デストラクタ。サーバーソケットを正しくクローズし、すべてのクライアントを削除する。
 */
Server::~Server() {
    if (_server_fd != -1) {
        close(_server_fd);
    }
    // 念のためクライアントをすべてクローズ
    for (size_t i = 0; i < _client_fds.size(); ++i) {
        close(_client_fds[i]);
    }
    _client_fds.clear();
    _clients.clear();
}

/**
 * @brief ノンブロッキングモードに設定
 */
static bool setNonBlocking(int fd) {
    // 要件に合わせた実装：直接O_NONBLOCKを設定
    if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
        return false;
    }
    return true;
}

/**
 * @brief サーバーを起動し、クライアントからの接続を受け入れるメインループを実行。
 *        非ブロッキングソケットを用い、selectを1か所のみ使用して管理する。
 */
void Server::start() {
    // ソケット作成
    _server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_server_fd < 0) {
        std::cerr << "Socket creation failed: " << strerror(errno) << std::endl;
        return;
    }
    // ノンブロッキングに設定
    if (!setNonBlocking(_server_fd)) {
        std::cerr << "Failed to set server socket to non-blocking." << std::endl;
        close(_server_fd);
        _server_fd = -1;
        return;
    }

    // アドレス再利用設定（アプリ終了直後などにすぐ使いやすくするため）
    int opt = 1;
    if (setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        std::cerr << "setsockopt failed: " << strerror(errno) << std::endl;
        close(_server_fd);
        _server_fd = -1;
        return;
    }

    // サーバーのアドレス情報を設定
    sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(_port);

    // ソケットにアドレスをバインド
    if (bind(_server_fd, (struct sockaddr*)&address, sizeof(address)) == -1) {
        std::cerr << "Bind failed: " << strerror(errno) << std::endl;
        close(_server_fd);
        _server_fd = -1;
        return;
    }

    // ソケットをリスニング状態に設定
    if (listen(_server_fd, 10) == -1) {
        std::cerr << "Listen failed: " << strerror(errno) << std::endl;
        close(_server_fd);
        _server_fd = -1;
        return;
    }

    std::cout << "Server started on port " << _port << std::endl;

    // メインループ：selectを用いてクライアントFDとサーバーFDを同時に監視
    while (true) {
        // fd_setを毎ループ初期化
        FD_ZERO(&_read_fds);
        FD_SET(_server_fd, &_read_fds);
        int max_fd = _server_fd;

        // クライアントFDの追加
        for (size_t i = 0; i < _client_fds.size(); ++i) {
            int fd = _client_fds[i];
            FD_SET(fd, &_read_fds);
            if (fd > max_fd) {
                max_fd = fd;
            }
        }

        // selectで監視
        int activity = select(max_fd + 1, &_read_fds, NULL, NULL, NULL);
        if (activity < 0 && errno != EINTR) {
            std::cerr << "Select error: " << strerror(errno) << std::endl;
        }

        // 新規接続
        if (FD_ISSET(_server_fd, &_read_fds)) {
            acceptClient();
        }

        // 各クライアントのハンドリング
        for (size_t i = 0; i < _client_fds.size(); ++i) {
            int fd = _client_fds[i];
            if (FD_ISSET(fd, &_read_fds)) {
                handleClient(fd);
            }
        }
    }
}

/**
 * @brief 新しいクライアント接続を受け入れる。accept後、認証処理を行い、
 *        認証成功したらクライアントリストに追加し、対応バッファを初期化する。
 */
void Server::acceptClient() {
    sockaddr_in client_address;
    socklen_t client_address_len = sizeof(client_address);
    int client_fd = accept(_server_fd, (struct sockaddr*)&client_address, &client_address_len);
    if (client_fd < 0) {
        if (errno != EWOULDBLOCK && errno != EAGAIN) {
            std::cerr << "Accept failed: " << strerror(errno) << std::endl;
        }
        return;
    }

    // クライアントソケットをノンブロッキングに
    if (!setNonBlocking(client_fd)) {
        std::cerr << "Failed to set client socket to non-blocking. Closing." << std::endl;
        close(client_fd);
        return;
    }

    // クライアント追加（認証前の初期状態）
    _client_fds.push_back(client_fd);
    _clients[client_fd] = ClientInfo(); // デフォルトコンストラクタでauthenticated=false, password_sent=falseに
    _clientBuffers[client_fd] = std::string();
    
    std::cout << "New client connected: " << client_fd << std::endl;
    
    // パスワードプロンプトを送信
    const std::string password_prompt = "Enter server password: ";
    send(client_fd, password_prompt.c_str(), password_prompt.size(), 0);
    _clients[client_fd].password_sent = true;
}


/**
 * @brief クライアントの認証を行う。パスワードを送受信して接続パスワードと比較。
 */
bool Server::authenticateClient(int client_fd) {
    char buffer[1024];
    std::memset(buffer, 0, sizeof(buffer));
    int valread = recv(client_fd, buffer, sizeof(buffer), 0);

    if (valread < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // まだデータが届いていない、次のselectで再試行
            return true; // 接続は維持
        } else {
            std::cerr << "Failed to read password from client: " << strerror(errno) << std::endl;
            return false;
        }
    }
    
    if (valread == 0) {
        // クライアント切断
        std::cerr << "Client disconnected before sending password." << std::endl;
        return false;
    }

    std::string received_password(buffer, valread);
    size_t trim_pos = received_password.find_last_not_of(" \n\r\t");
    if (trim_pos != std::string::npos) {
        received_password.erase(trim_pos + 1);
    }

    if (received_password != _password) {
        const std::string error_message = "Incorrect password. Connection closed.\n";
        send(client_fd, error_message.c_str(), error_message.size(), 0);
        return false;
    }

    const std::string success_message = "Password accepted. Welcome!\n";
    send(client_fd, success_message.c_str(), success_message.size(), 0);
    _clients[client_fd].authenticated = true; // 認証成功
    return true;
}


/**
 * @brief クライアントからのデータを読み取り、\n区切りでコマンドに分割。各コマンドを処理する。
 */
void Server::handleClient(int client_fd) {
    // 認証されていない場合、まず認証を試みる
    if (!_clients[client_fd].authenticated) {
        if (!authenticateClient(client_fd)) {
            removeClient(client_fd);
            return;
        }
        
        // まだ認証できていない場合は次のselectまで待機
        if (!_clients[client_fd].authenticated) {
            return;
        }
    }
    
    // 認証済みクライアントの通常の処理
    // 一時的に受信バッファに読み込み
    char tempBuf[1024];
    std::memset(tempBuf, 0, sizeof(tempBuf));
    int valread = recv(client_fd, tempBuf, sizeof(tempBuf), 0);
    
    if (valread < 0) {
        // EAGAIN/EWOULDBLOCKはエラーではなく、単にデータがまだ来ていないだけ
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return; // エラーメッセージ表示なし
        }
        // その他の実際のエラーの場合
        removeClient(client_fd);
        return;
    }
    else if (valread == 0) {
        // クライアントが正常に切断した場合（Ctrl+CやTelnetのquit等）
        // エラーメッセージなし - removeClientの中の表示だけにする
        removeClient(client_fd);
        return;
    }

    std::string &bufRef = _clientBuffers[client_fd];
    bufRef.append(tempBuf, valread);

    // 改行(\n)単位でコマンドを切り出す
    size_t pos;
    while ((pos = bufRef.find('\n')) != std::string::npos) {
        // 1行分のコマンド
        std::string line = bufRef.substr(0, pos);
        bufRef.erase(0, pos + 1);

        // コマンドが空の場合スキップ
        if (line.empty()) {
            continue;
        }


        // コマンド文字列を解析
        std::istringstream iss(line);
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
            std::string password;
            iss >> channel_name >> password;
            handleJoinCommand(client_fd, channel_name, password);
        } else if (command == "PRIVMSG") {
            std::string target;
            iss >> target;
            // メッセージの残りをすべて取得
            std::string message;
            std::getline(iss, message);
            if (!message.empty() && message[0] == ' ') {
                message.erase(0, 1);
            }
            handlePrivmsgCommand(client_fd, target, message);
        } else if (command == "KICK") {
            std::string channel_name, target_nickname;
            iss >> channel_name >> target_nickname;
            handleKickCommand(client_fd, channel_name, target_nickname);
        } else if (command == "MODE") {
            std::string channel_name, mode, parameter;
            iss >> channel_name >> mode >> parameter;
            handleModeCommand(client_fd, channel_name, mode, parameter);
        } else if (command == "INVITE") {
            std::string target_nickname, channel_name;
            iss >> target_nickname >> channel_name;
            handleInviteCommand(client_fd, target_nickname, channel_name);
        } else if (command == "TOPIC") {
            // トピックを変更または取得
            std::string channel_name;
            iss >> channel_name; 
            std::string new_topic;
            std::getline(iss, new_topic);
            if (!new_topic.empty() && new_topic[0] == ' ') {
                new_topic.erase(0, 1);
            }
            handleTopicCommand(client_fd, channel_name, new_topic);
        } else {
            const std::string error_message = "Unknown command.\n";
            send(client_fd, error_message.c_str(), error_message.size(), 0);
        }
    }
}

/**
 * @brief クライアントのニックネームを設定。
 */
void Server::handleNickCommand(int client_fd, const std::string &nickname) {
    _clients[client_fd].nickname = nickname;
    std::string response = "Nickname set to " + nickname + "\n";
    send(client_fd, response.c_str(), response.size(), 0);
}

/**
 * @brief クライアントのユーザー名を設定する。
 */
void Server::handleUserCommand(int client_fd, const std::string &username) {
    _clients[client_fd].username = username;
    std::string response = "Username set to " + username + "\n";
    send(client_fd, response.c_str(), response.size(), 0);
}

/**
 * @brief クライアントをチャネルに参加させるコマンドJOINの処理。
 */
void Server::handleJoinCommand(int client_fd, const std::string &channel_name, const std::string &password) {
    // std::cout << "DEBUG JOIN: channel=" << channel_name << " password='" << password << "'" << std::endl;
    
    if (_channels.find(channel_name) == _channels.end()) {
        createChannel(channel_name, client_fd);
    } else {
        // +iモードのチェック - 既存
        if (_channels[channel_name].hasMode('i') && 
            !_channels[channel_name].isInvitee(client_fd)) {
            std::string error_message = "Cannot join channel (+i)\n";
            send(client_fd, error_message.c_str(), error_message.size(), 0);
            return;
        }
        
        // +kモードのチェック - 既存
        if (_channels[channel_name].hasMode('k') && 
            !_channels[channel_name].checkPassword(password)) {
            std::string error_message = "Cannot join channel (wrong password)\n";
            send(client_fd, error_message.c_str(), error_message.size(), 0);
            return;
        }
        
        // +lモードのチェック - 追加
        if (_channels[channel_name].hasMode('l') && 
            _channels[channel_name].isUserLimitReached()) {
            std::string error_message = "Cannot join channel (+l): user limit reached\n";
            send(client_fd, error_message.c_str(), error_message.size(), 0);
            return;
        }
    }
    joinChannel(client_fd, channel_name);
}

/**
 * @brief PRIVMSGコマンドの処理。ターゲットがチャネルかユーザーで分岐。
 */
void Server::handlePrivmsgCommand(int client_fd,
                            const std::string &target,
                            const std::string &message) {
    if (_channels.find(target) != _channels.end()) {
        // チャネルに送信
        const std::vector<int>& clients = _channels[target].getClients();

        // チャンネルのメンバーでない場合
        if (std::find(clients.begin(), clients.end(), client_fd) == clients.end()) {
            std::string error_message = "You are not in channel: " + target + "\n";
            send(client_fd, error_message.c_str(), error_message.size(), 0);
            return;
        }

        if (_channels[target].hasMode('m') &&
            !_channels[target].isOperator(client_fd)) {
            std::string error_message = "Channel is moderated. Only operators can send messages.\n";
            send(client_fd, error_message.c_str(), error_message.size(), 0);
            return;
        }
        // 同じチャネルの他のクライアントにメッセージを転送
        for (size_t i = 0; i < clients.size(); ++i) {
            if (clients[i] != client_fd) {
                std::string full_message = _clients[client_fd].nickname + ": " + message + "\n";
                send(clients[i], full_message.c_str(), full_message.size(), 0);
            }
        }
    } else {
        // 個人に送信
        int target_fd = -1;
        for (std::map<int, ClientInfo>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
            if (it->second.nickname == target) {
                target_fd = it->first;
                break;
            }
        }
        if (target_fd == -1) {
            std::string error_message = "No such user or channel: " + target + "\n";
            send(client_fd, error_message.c_str(), error_message.size(), 0);
            return;
        }
        std::string full_message = _clients[client_fd].nickname + ": " + message + "\n";
        send(target_fd, full_message.c_str(), full_message.size(), 0);
    }
}

/**
 * @brief INVITEコマンドの処理。指定ユーザーをチャネルに招待する。
 */
void Server::handleInviteCommand(int client_fd,
                                 const std::string &target_nickname,
                                 const std::string &channel_name) {
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
        std::string error_message = "No such user or channel: " + target_nickname + "\n";
        send(client_fd, error_message.c_str(), error_message.size(), 0);
        return;
    }

    _channels[channel_name].addInvitee(target_fd);
    std::string response = "User " + target_nickname + " has been invited to channel " + channel_name + "\n";
    send(client_fd, response.c_str(), response.size(), 0);
    send(target_fd, response.c_str(), response.size(), 0);
}

/**
 * @brief チャネルを作成し、作成者をオペレーターにする。
 */
void Server::createChannel(const std::string &channel_name, int client_fd) {
    _channels[channel_name] = Channel(channel_name);
    _channels[channel_name].addOperator(client_fd);
    std::cout << "Channel created: " << channel_name << std::endl;
}

/**
 * @brief クライアントを既存のチャネルに参加させる。
 */
void Server::joinChannel(int client_fd, const std::string &channel_name) {
    _channels[channel_name].addClient(client_fd);
    std::string response = "Joined channel " + channel_name + "\n";
    send(client_fd, response.c_str(), response.size(), 0);
}

/**
 * @brief KICKコマンドの処理。特定ユーザーをチャネルから強制退出させる。
 */
void Server::handleKickCommand(int client_fd,
                               const std::string &channel_name,
                               const std::string &target_nickname) {
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
        std::string error_message = "No such user or channel: " + target_nickname + "\n";
        send(client_fd, error_message.c_str(), error_message.size(), 0);
        return;
    }

    _channels[channel_name].removeClient(target_fd);
    std::string response = "User " + target_nickname + " has been kicked from channel " + channel_name + "\n";
    send(client_fd, response.c_str(), response.size(), 0);
    send(target_fd, response.c_str(), response.size(), 0);
}

/**
 * @brief MODEコマンドの処理。チャンネルモードを追加・削除する。
 */
void Server::handleModeCommand(int client_fd,
                                const std::string &channel_name,
                                const std::string &mode,
                                const std::string &parameter) {
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

    if (mode.size() >= 2) {
        if (mode[0] == '+') {
            // +kモードの場合、パスワードが必須
            if (mode[1] == 'k') {
                if (parameter.empty()) {
                    // パスワードが指定されていない場合はエラー
                    std::string error_message = "MODE +k requires a password parameter\n";
                    send(client_fd, error_message.c_str(), error_message.size(), 0);
                    return; // モード設定せずに終了
                }
                // パスワードがある場合は正常処理
                _channels[channel_name].addMode(mode[1]);
                _channels[channel_name].setPassword(parameter);
                std::cout << "DEBUG: Set password for " << channel_name << ": '" << parameter << "'" << std::endl;
            } 
            // +lモードの場合、数値パラメータが必須
            else if (mode[1] == 'l') {
                if (parameter.empty()) {
                    // ユーザー数上限が指定されていない場合はエラー
                    std::string error_message = "MODE +l requires a numeric parameter\n";
                    send(client_fd, error_message.c_str(), error_message.size(), 0);
                    return; // モード設定せずに終了
                }

                // パラメータが数値かチェック
                int limit = 0;
                try {
                    limit = std::atoi(parameter.c_str());
                } catch (std::exception &e) {
                    std::string error_message = "MODE +l requires a valid numeric parameter\n";
                    send(client_fd, error_message.c_str(), error_message.size(), 0);
                    return;
                }

                // 正の値かチェック
                if (limit <= 0) {
                    std::string error_message = "User limit must be a positive number\n";
                    send(client_fd, error_message.c_str(), error_message.size(), 0);
                    return;
                }
                // 上限を設定
                _channels[channel_name].addMode(mode[1]);
                _channels[channel_name].setUserLimit(limit);
                std::cout << "DEBUG: Set user limit for " << channel_name << " to " << limit << std::endl;
            } 
            else if (mode[1] == 'o') {
                if (parameter.empty()) {
                    std::string error_message = "MODE +o requires a nickname parameter\n";
                    send(client_fd, error_message.c_str(), error_message.size(), 0);
                    return;
                }
                
                // 指定されたユーザーが存在するか確認
                int target_fd = -1;
                for (std::map<int, ClientInfo>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
                    if (it->second.nickname == parameter) {
                        target_fd = it->first;
                        break;
                    }
                }
                
                if (target_fd == -1) {
                    std::string error_message = "No such user or channel: " + parameter + "\n";
                    send(client_fd, error_message.c_str(), error_message.size(), 0);
                    return;
                }
                
                // ユーザーがチャンネルのメンバーか確認
                const std::vector<int>& clients = _channels[channel_name].getClients();
                if (std::find(clients.begin(), clients.end(), target_fd) == clients.end()) {
                    std::string error_message = "User " + parameter + " is not in channel " + channel_name + "\n";
                    send(client_fd, error_message.c_str(), error_message.size(), 0);
                    return;
                }
                
                // オペレータ権限を付与
                _channels[channel_name].addOperator(target_fd);
                _channels[channel_name].addMode(mode[1]);  // モードは必要に応じて
                std::cout << "DEBUG: Set operator " << parameter << " for " << channel_name << std::endl;
            } else {
                // その他のモードは通常通り設定
                _channels[channel_name].addMode(mode[1]);
            }
        } else if (mode[0] == '-') {
            _channels[channel_name].removeMode(mode[1]);
            // -kモードの場合はパスワードをクリア
            if (mode[1] == 'k') {
                _channels[channel_name].setPassword("");
            }
            // -lモードの場合はユーザー数上限をクリア
            else if (mode[1] == 'l') {
                _channels[channel_name].setUserLimit(0); // 0は無制限
            }
            else if (mode[1] == 'o') {
                if (parameter.empty()) {
                    std::string error_message = "MODE -o requires a nickname parameter\n";
                    send(client_fd, error_message.c_str(), error_message.size(), 0);
                    return;
                }
                
                int target_fd = -1;
                for (std::map<int, ClientInfo>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
                    if (it->second.nickname == parameter) {
                        target_fd = it->first;
                        break;
                    }
                }
                
                if (target_fd == -1) {
                    std::string error_message = "No such user or channel: " + parameter + "\n";
                    send(client_fd, error_message.c_str(), error_message.size(), 0);
                    return;
                }
                
                // オペレータ権限を剥奪
                _channels[channel_name].removeOperator(target_fd);
                _channels[channel_name].removeMode(mode[1]);  // モードは必要に応じて
            }
            else {
                _channels[channel_name].removeMode(mode[1]);
            }
        }
    }

    std::string response = "Channel mode for " + channel_name + " changed to " + mode + "\n";
    send(client_fd, response.c_str(), response.size(), 0);
}

/**
 * @brief TOPICコマンドの処理。トピックを設定・取得する。
 *        +tモードの場合、オペレーターのみトピックを変更可能。
 */
void Server::handleTopicCommand(int client_fd,
                                const std::string &channel_name,
                                const std::string &new_topic) {
    if (_channels.find(channel_name) == _channels.end()) {
        std::string error_message = "No such channel: " + channel_name + "\n";
        send(client_fd, error_message.c_str(), error_message.size(), 0);
        return;
    }
    // トピックの取得・設定
    Channel &ch = _channels[channel_name];
    if (new_topic.empty()) {
        // 取得
        std::string response = "Topic of " + channel_name + ": " + ch.getTopic() + "\n";
        send(client_fd, response.c_str(), response.size(), 0);
    } else {
        // +tが付いていて、かつオペレーター以外は変更不可
        if (ch.hasMode('t') && !ch.isOperator(client_fd)) {
            std::string error_message = "Topic change is restricted (+t).\n";
            send(client_fd, error_message.c_str(), error_message.size(), 0);
            return;
        }
        ch.setTopic(new_topic);
        std::string response = "Topic for " + channel_name + " is set to: " + new_topic + "\n";
        send(client_fd, response.c_str(), response.size(), 0);
    }
}

/**
 * @brief クライアント接続を終了し、管理構造から削除する。
 */
void Server::removeClient(int client_fd) {
    close(client_fd);
    _client_fds.erase(std::remove(_client_fds.begin(), _client_fds.end(), client_fd),
                      _client_fds.end());
    _clients.erase(client_fd);
    _clientBuffers.erase(client_fd);
    std::cout << "Client disconnected: " << client_fd << std::endl;
}

/**
 * @brief チャネルにユーザーを招待する（内部用）。
 *        handleInviteCommand()を使用しない別箇所での呼び出し用。
 */
void Server::inviteUser(int client_fd,
                        const std::string &channel_name,
                        const std::string &target_nickname) {
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
        std::string error_message = "No such user or channel: " + target_nickname + "\n";
        send(client_fd, error_message.c_str(), error_message.size(), 0);
        return;
    }

    _channels[channel_name].addInvitee(target_fd);
    std::string response = "User " + target_nickname + " has been invited to channel " + channel_name + "\n";
    send(client_fd, response.c_str(), response.size(), 0);
    send(target_fd, response.c_str(), response.size(), 0);
}

