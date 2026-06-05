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

bool netStartup();
void netShutdown();
bool setNonBlocking(SOCKET socketHandle);
bool parseAddress(const std::string& text, sockaddr_in& out);
std::string addressToString(const sockaddr_in& addr);
uint64_t nowMs();

} // namespace MimitaNet
