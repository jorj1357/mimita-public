// 2026-11-13
#pragma once

#include <cstdint>
#include <winsock2.h>

#include "host/abi/game-host-abi.h"

namespace mimita::game {
void initializeHostSocket(const mimita::host::HostServicesV1* services);
SOCKET openUdpSocket();
void closeSocket(SOCKET socketHandle);
bool bindSocket(SOCKET socketHandle, const sockaddr_in* address);
bool getSocketLocalAddress(SOCKET socketHandle, sockaddr_in* address);
bool setSocketNonBlocking(SOCKET socketHandle);
bool setSocketReuse(SOCKET socketHandle, bool enabled);
bool setSocketExclusive(SOCKET socketHandle, bool enabled);
bool setSocketRecvTimeout(SOCKET socketHandle, std::uint32_t milliseconds);
bool sendToSocket(SOCKET socketHandle, const void* data, std::uint32_t size,
                  const sockaddr_in* address);
bool recvFromSocket(SOCKET socketHandle, void* data, std::uint32_t capacity,
                    sockaddr_in* address, std::uint32_t* received,
                    bool* wouldBlock);
}  // namespace mimita::game
