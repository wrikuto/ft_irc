#include "../include/server.hpp"
#include <iostream>
#include <unistd.h> // sleep関数のために追加

int main() {
    std::string password;
    std::cout << "Enter server password: ";
    std::cin >> password;

    Server server(6669, password);
    server.start();
    while (1){
        sleep(1); // CPU使用率を下げるために1秒待機
    }
    return 0;
}