#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")

const char* END_MARKER = "<END_OF_RESPONSE>\n";

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "socket() failed" << std::endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(54000);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "connect() failed" << std::endl;
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to SQL server. Type SQL commands (end with ;). Type EXIT; to quit." << std::endl;

    std::string line;
    while (true) {
        std::cout << "SQL> ";
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;
        std::string tosend = line + "\n";
        send(sock, tosend.c_str(), static_cast<int>(tosend.size()), 0);

        // receive until END_MARKER
        std::string acc;
        char buf[1024];
        while (true) {
            int bytes = recv(sock, buf, sizeof(buf), 0);
            if (bytes <= 0) break;
            acc.append(buf, buf + bytes);
            size_t p = acc.find(END_MARKER);
            if (p != std::string::npos) {
                std::string resp = acc.substr(0, p);
                std::cout << resp;
                acc.erase(0, p + strlen(END_MARKER));
                break;
            }
        }

        if (line == "EXIT;" || line == "exit;") break;
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
