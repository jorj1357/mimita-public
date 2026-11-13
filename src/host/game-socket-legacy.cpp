// 2026-11-13
// Transitional EXE-side UDP socket implementation for pre-cutover game code.

#include <winsock2.h>

#include "game/socket-bridge-module.h"

namespace mimita::game {
SOCKET openUdpSocket() {
    return ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

void closeSocket(SOCKET socketHandle) {
    if (socketHandle != INVALID_SOCKET) ::closesocket(socketHandle);
}

bool bindSocket(SOCKET socketHandle, const sockaddr_in* address) {
    return socketHandle != INVALID_SOCKET && address &&
           ::bind(socketHandle, reinterpret_cast<const sockaddr*>(address),
                  sizeof(sockaddr_in)) == 0;
}

bool getSocketLocalAddress(SOCKET socketHandle, sockaddr_in* address) {
    if (socketHandle == INVALID_SOCKET || !address) return false;
    int length = sizeof(sockaddr_in);
    return ::getsockname(socketHandle, reinterpret_cast<sockaddr*>(address), &length) == 0;
}

bool setSocketNonBlocking(SOCKET socketHandle) {
    u_long enabled = 1;
    return ::ioctlsocket(socketHandle, FIONBIO, &enabled) == 0;
}

bool setSocketReuse(SOCKET socketHandle, bool enabled) {
    BOOL value = enabled;
    return ::setsockopt(socketHandle, SOL_SOCKET, SO_REUSEADDR,
                        reinterpret_cast<const char*>(&value), sizeof(value)) == 0;
}

bool setSocketExclusive(SOCKET socketHandle, bool enabled) {
    BOOL value = enabled;
    return ::setsockopt(socketHandle, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                        reinterpret_cast<const char*>(&value), sizeof(value)) == 0;
}

bool setSocketRecvTimeout(SOCKET socketHandle, std::uint32_t milliseconds) {
    DWORD value = milliseconds;
    return ::setsockopt(socketHandle, SOL_SOCKET, SO_RCVTIMEO,
                        reinterpret_cast<const char*>(&value), sizeof(value)) == 0;
}

bool sendToSocket(SOCKET socketHandle, const void* data, std::uint32_t size,
                  const sockaddr_in* address) {
    return ::sendto(socketHandle, static_cast<const char*>(data), static_cast<int>(size),
                    0, reinterpret_cast<const sockaddr*>(address),
                    sizeof(sockaddr_in)) != SOCKET_ERROR;
}

bool recvFromSocket(SOCKET socketHandle, void* data, std::uint32_t capacity,
                    sockaddr_in* address, std::uint32_t* received, bool* wouldBlock) {
    int length = sizeof(sockaddr_in);
    const int result = ::recvfrom(socketHandle, static_cast<char*>(data),
                                  static_cast<int>(capacity), 0,
                                  reinterpret_cast<sockaddr*>(address), &length);
    if (result == SOCKET_ERROR) {
        if (WSAGetLastError() == WSAEWOULDBLOCK) {
            if (wouldBlock) *wouldBlock = true;
            if (received) *received = 0;
            return true;
        }
        return false;
    }
    if (wouldBlock) *wouldBlock = false;
    if (received) *received = static_cast<std::uint32_t>(result);
    return true;
}
}  // namespace mimita::game
