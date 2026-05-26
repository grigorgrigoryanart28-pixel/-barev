#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

typedef SOCKET SocketHandle;

#define INVALID_SOCKET_FD INVALID_SOCKET
#define SOCKET_ERROR_FD SOCKET_ERROR
#define CLOSE_SOCKET(s) closesocket(s)
#define WSA_INIT() WSAStartup(MAKEWORD(2, 2), &wsaData)
#define WSA_CLEANUP() WSACleanup()
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>

typedef int SocketHandle;

#define INVALID_SOCKET_FD (-1)
#define SOCKET_ERROR_FD (-1)
#define CLOSE_SOCKET(s) close(s)
#define WSA_INIT() 0
#define WSA_CLEANUP() \
	do                  \
	{                   \
	} while (0)
#endif

#define IS_INVALID_SOCKET(sock) ((sock) == INVALID_SOCKET_FD)
#define IS_SOCKET_ERROR(code) ((code) == SOCKET_ERROR_FD)
