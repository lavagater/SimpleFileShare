#ifndef WYATT_SOCK_H
#define WYATT_SOCK_H
#include <stdlib.h>  /*exit, calloc, free*/
#ifdef _WIN32
#include <WinSock2.h> /*sockets*/
#else
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>
typedef int SOCKET;
#define SD_SEND SHUT_WR
#define SOCKET_ERROR -1
#define INVALID_SOCKET -1
#endif
#define EBLOCK -6969

void SetNonBlocking(SOCKET sock, int IsBlocking);
void SetNonBlocking(SOCKET sock);
int GetError();
void Close(SOCKET sock, bool now);
int Send(SOCKET sock, const char* buffer, int bytes, sockaddr_in* dest);
int Receive(SOCKET sock, char* buffer, int maxBytes);
int Receive(SOCKET sock, char* buffer, int maxBytes, sockaddr_in *addr);
int Bind(SOCKET sock, sockaddr_in* addr);
sockaddr_in* CreateAddress(const char* ip, int port);
void CreateAddress(const char* ip, int port, sockaddr_in *addr);
SOCKET CreateSocket(int protocol);
int Connect(SOCKET sock, sockaddr_in* address);
int SendTCP(SOCKET sock, const char* buffer, int bytes);
int ReceiveTCP(SOCKET sock, char* buffer, int maxBytes);
int Init();
void Deinit();
#endif