#include "../include/server.hpp"
#include <iostream>
#include <cstdlib>    // atoiに必要
#include <unistd.h>   // sleep関数に必要

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <port> <password>\n";
        return 1;
    }

    // 引数からポート番号とパスワードを取得
    int port = std::atoi(argv[1]);
    std::string password = argv[2];

    Server server(port, password);
    server.start();

    while (true) {
        sleep(1);
    }

    return 0;
}