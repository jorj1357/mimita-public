// Resident UDP socket primitive for the strict cold EXE.
//
// The game generation owns the strong bridge implementation. These weak
// definitions keep the resident server/ICE loop linkable and use the OS socket
// primitive directly; the game-owned bridge wins in the migration EXE.

#include "game/socket-bridge-module.h"

#include <winsock2.h>

namespace mimita::game {

__attribute__((weak)) SOCKET openUdpSocket() {
    return ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

__attribute__((weak)) void closeSocket(SOCKET socketHandle) {
    if (socketHandle != INVALID_SOCKET) ::closesocket(socketHandle);
}

__attribute__((weak)) bool bindSocket(SOCKET socketHandle,
                                      const sockaddr_in* address) {
    return address && ::bind(socketHandle,
        reinterpret_cast<const sockaddr*>(address), sizeof(sockaddr_in)) == 0;
}

__attribute__((weak)) bool getSocketLocalAddress(SOCKET socketHandle,
                                                 sockaddr_in* address) {
    int length = sizeof(sockaddr_in);
    return address && ::getsockname(socketHandle,
        reinterpret_cast<sockaddr*>(address), &length) == 0;
}

__attribute__((weak)) bool setSocketNonBlocking(SOCKET socketHandle) {
    u_long enabled = 1;
    return ::ioctlsocket(socketHandle, FIONBIO, &enabled) == 0;
}

__attribute__((weak)) bool setSocketReuse(SOCKET socketHandle, bool enabled) {
    const BOOL value = enabled ? TRUE : FALSE;
    return ::setsockopt(socketHandle, SOL_SOCKET, SO_REUSEADDR,
        reinterpret_cast<const char*>(&value), sizeof(value)) == 0;
}

__attribute__((weak)) bool setSocketExclusive(SOCKET socketHandle,
                                              bool enabled) {
    const BOOL value = enabled ? FALSE : TRUE;
    return ::setsockopt(socketHandle, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
        reinterpret_cast<const char*>(&value), sizeof(value)) == 0;
}

__attribute__((weak)) bool setSocketRecvTimeout(SOCKET socketHandle,
                                                std::uint32_t milliseconds) {
    const DWORD value = milliseconds;
    return ::setsockopt(socketHandle, SOL_SOCKET, SO_RCVTIMEO,
        reinterpret_cast<const char*>(&value), sizeof(value)) == 0;
}

__attribute__((weak)) bool sendToSocket(SOCKET socketHandle, const void* data,
                                        std::uint32_t size,
                                        const sockaddr_in* address) {
    return address && ::sendto(socketHandle,
        static_cast<const char*>(data), static_cast<int>(size), 0,
        reinterpret_cast<const sockaddr*>(address), sizeof(sockaddr_in))
        == static_cast<int>(size);
}

__attribute__((weak)) bool recvFromSocket(SOCKET socketHandle, void* data,
                                          std::uint32_t capacity,
                                          sockaddr_in* address,
                                          std::uint32_t* received,
                                          bool* wouldBlock) {
    int length = sizeof(sockaddr_in);
    const int result = ::recvfrom(socketHandle, static_cast<char*>(data),
        static_cast<int>(capacity), 0, reinterpret_cast<sockaddr*>(address),
        &length);
    if (received) *received = result > 0 ? static_cast<std::uint32_t>(result) : 0;
    if (wouldBlock) {
        *wouldBlock = result == SOCKET_ERROR &&
            WSAGetLastError() == WSAEWOULDBLOCK;
    }
    return result >= 0;
}

}  // namespace mimita::game
