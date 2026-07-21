// 07 21 2026, 18 32
/* purpose
* Owns shared WinSock startup, endpoint parsing, and socket utility helpers.
* Provides small transport primitives reused by UDP server, client, and tests.
* Keeps IPv4 endpoint formatting consistent in logs and harnesses.
* Does NOT own packet schemas, game transport policy, or coordinator behavior.
* Does NOT perform DNS resolution, ICE negotiation, or gameplay simulation.
* Does NOT start servers, clients, threads, or rendering systems.
*/

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

// Normalize a map identifier: strip directory prefix, strip .glb, lowercase.
std::string normalizeMapId(const std::string& mapId);
bool mapIdsReferToSameMap(const std::string& a, const std::string& b);
bool setNonBlocking(SOCKET socketHandle);
bool parseAddress(const std::string& text, sockaddr_in& out, bool allowPortZero = false);
std::string addressToString(const sockaddr_in& addr);
uint64_t nowMs();

} // namespace MimitaNet
