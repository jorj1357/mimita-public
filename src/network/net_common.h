#pragma once

#include <cstdint>
#include <string>

namespace MimitaNet {

constexpr uint16_t DEFAULT_PORT = 1357;

#ifdef _WIN32

using Socket = SOCKET;

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

using Socket = SOCKET;
constexpr Socket INVALID_SOCKET_HANDLE = INVALID_SOCKET;
#else

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

using Socket = int;
constexpr Socket INVALID_SOCKET_HANDLE = -1;
#endif

bool netStartup();
void netShutdown();
bool setNonBlocking(Socket socketHandle);
bool parseAddress(const std::string& text, sockaddr_in& out);
std::string addressToString(const sockaddr_in& addr);
uint64_t nowMs();

} // namespace MimitaNet
