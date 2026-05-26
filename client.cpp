#include "network_compat.h"
#include <iostream>
#include <string>
#include <vector>
#include <cstring>

const char *END_MARKER = "<END_OF_RESPONSE>\n";

int main()
{
#ifdef _WIN32
    WSADATA wsaData;
#endif
    if (WSA_INIT() != 0)
    {
        std::cerr << "WSAStartup failed" << std::endl;
        return 1;
    }

    SocketHandle sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (IS_INVALID_SOCKET(sock))
    {
        std::cerr << "socket() failed" << std::endl;
        WSA_CLEANUP();
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(54000);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0 .1");

    if (connect(sock, (sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR_FD)
    {
        std::cerr << "connect() failed" << std::endl;
        CLOSE_SOCKET(sock);
        WSA_CLEANUP();
        return 1;
    }

    std::cout << "Connected to SQL server. Type SQL commands (end with ;). Type EXIT; to quit." << std::endl;

    std::string line;
    while (true)
    {
        std::cout << "SQL> ";
        if (!std::getline(std::cin, line))
            break;
        if (line.empty())
            continue;
        std::string tosend = line + "\n";
        send(sock, tosend.c_str(), static_cast<int>(tosend.size()), 0);

        // receive until END_MARKER
        std::string acc;
        char buf[1024];
        while (true)
        {
            int bytes = recv(sock, buf, sizeof(buf), 0);
            if (bytes <= 0)
                break;
            acc.append(buf, buf + bytes);
            size_t p = acc.find(END_MARKER);
            if (p != std::string::npos)
            {
                std::string resp = acc.substr(0, p);
                std::cout << resp;
                acc.erase(0, p + strlen(END_MARKER));
                break;
            }
        }

        if (line == "EXIT;" || line == "exit;")
            break;
    }

    CLOSE_SOCKET(sock);
    WSA_CLEANUP();
    return 0;
}
