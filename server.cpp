#define SQL_SERVER 1
#include "network_compat.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <thread>
#include <mutex>
#include <string>

#include "SQL.cpp"

const char *END_MARKER = "<END_OF_RESPONSE>\n";

void handle_client(SocketHandle clientSock, SQL &db, std::mutex &db_mutex)
{
    char buffer[1024];
    std::string incoming;
    while (true)
    {
        int bytes = recv(clientSock, buffer, sizeof(buffer), 0);
        if (bytes <= 0)
            break;
        incoming.append(buffer, buffer + bytes);
        // process lines separated by '\n'
        size_t pos;
        while ((pos = incoming.find('\n')) != std::string::npos)
        {
            std::string line = incoming.substr(0, pos);
            incoming.erase(0, pos + 1);
            line = trim(line);
            if (line.empty())
                continue;
            std::cout << "Received client command: " << line << std::endl;
            if (line.rfind("CREATE TABLE", 0) == 0 || line.rfind("create table", 0) == 0)
            {
                std::cout << "Client has created table." << std::endl;
            }
            if (line == "EXIT;" || line == "exit;")
            {
                CLOSE_SOCKET(clientSock);
                std::cout << "Client disconnected" << std::endl;
                return;
            }

            // capture cout output
            std::ostringstream oss;
            {
                std::lock_guard<std::mutex> lock(db_mutex);
                auto oldbuf = std::cout.rdbuf(oss.rdbuf());
                SQL_compiler(db, line);
                std::cout.rdbuf(oldbuf);
            }

            std::string resp = oss.str();
            if (resp.empty())
                resp = "OK\n";
            resp += END_MARKER;
            send(clientSock, resp.c_str(), static_cast<int>(resp.size()), 0);
        }
    }
    CLOSE_SOCKET(clientSock);
    std::cout << "Client disconnected" << std::endl;
}

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

    SocketHandle listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (IS_INVALID_SOCKET(listenSock))
    {
        std::cerr << "socket() failed" << std::endl;
        WSA_CLEANUP();
        return 1;
    }

    sockaddr_in service;
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = inet_addr("127.0.0.1");
    service.sin_port = htons(54000);

    if (bind(listenSock, (sockaddr *)&service, sizeof(service)) == SOCKET_ERROR_FD)
    {
        std::cerr << "bind() failed" << std::endl;
        CLOSE_SOCKET(listenSock);
        WSA_CLEANUP();
        return 1;
    }

    if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR_FD)
    {
        std::cerr << "listen() failed" << std::endl;
        CLOSE_SOCKET(listenSock);
        WSA_CLEANUP();
        return 1;
    }

    SQL db;
    db.load_tables();
    std::mutex db_mutex;

    std::cout << "SQL server listening on 127.0.0.1:54000" << std::endl;

    while (true)
    {
        SocketHandle clientSock = accept(listenSock, nullptr, nullptr);
        if (IS_INVALID_SOCKET(clientSock))
        {
            std::cerr << "accept() failed" << std::endl;
            break;
        }
        std::cout << "Client connected" << std::endl;
        // Serve each client in its own detached thread so multiple clients
        // can connect and issue commands concurrently.
        try
        {
            std::thread clientThread(handle_client, clientSock, std::ref(db), std::ref(db_mutex));
            clientThread.detach();
        }
        catch (const std::system_error &e)
        {
            std::cerr << "Failed to create thread for client: " << e.what() << std::endl;
            CLOSE_SOCKET(clientSock);
        }
    }

    CLOSE_SOCKET(listenSock);
    WSA_CLEANUP(); 



    return 0;
}