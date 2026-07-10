#pragma once

#include <cstdint>
#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

namespace MimitaNet {

constexpr uint16_t DEFAULT_PORT = 1357;
constexpr uint64_t CLIENT_TIMEOUT_MS = 10000;
constexpr uint64_t SERVER_TIMEOUT_MS = 10000;

bool netStartup();
void netShutdown();
bool setNonBlocking(SOCKET socketHandle);
bool parseAddress(const std::string& text, sockaddr_in& out);
std::string addressToString(const sockaddr_in& addr);
uint64_t nowMs();

} // namespace MimitaNet
