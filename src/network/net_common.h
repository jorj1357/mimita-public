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

#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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

// Process network lifetime is resident; the game DLL forwards these through
// the transport-session capability.
bool netStartup();
void netShutdown();
bool setNonBlocking(SOCKET socketHandle);
bool disableUdpConnReset(SOCKET socketHandle);

// Pure helpers are inline so both the resident EXE and the replaceable game
// generation share one definition without a cold translation unit.
inline bool parseAddress(const std::string& text, sockaddr_in& out,
                         bool allowPortZero = false) {
    if (text.empty()) return false;
    if (text.front() == '[') return false;

    std::string host = text;
    uint16_t port = DEFAULT_PORT;

    const size_t colon = text.rfind(':');
    if (colon != std::string::npos) {
        host = text.substr(0, colon);
        const std::string portText = text.substr(colon + 1);
        if (portText.empty()) return false;
        for (const char c : portText) {
            if (!std::isdigit(static_cast<unsigned char>(c))) return false;
        }
        const unsigned long parsedPort =
            std::strtoul(portText.c_str(), nullptr, 10);
        if (parsedPort > 65535) return false;
        if (parsedPort == 0 && !allowPortZero) return false;
        port = static_cast<uint16_t>(parsedPort);
    }
    if (host == "localhost") host = "127.0.0.1";

    out = {};
    out.sin_family = AF_INET;
    out.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &out.sin_addr) != 1) return false;
    return true;
}

inline std::string addressToString(const sockaddr_in& addr) {
    char ip[INET_ADDRSTRLEN] = {};
    inet_ntop(AF_INET, const_cast<void*>(static_cast<const void*>(&addr.sin_addr)),
              ip, sizeof(ip));
    char out[64];
    snprintf(out, sizeof(out), "%s:%u", ip,
             static_cast<unsigned>(ntohs(addr.sin_port)));
    return std::string(out);
}

inline uint64_t nowMs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch())
            .count());
}

inline std::string normalizeMapId(const std::string& mapId) {
    std::string result = mapId;
    const size_t lastSlash = result.find_last_of("/\\");
    if (lastSlash != std::string::npos) result = result.substr(lastSlash + 1);
    if (result.size() > 4 && result.substr(result.size() - 4) == ".glb") {
        result = result.substr(0, result.size() - 4);
    }
    for (char& c : result) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return result;
}

inline bool mapIdsReferToSameMap(const std::string& a,
                                 const std::string& b) {
    return normalizeMapId(a) == normalizeMapId(b);
}

} // namespace MimitaNet
