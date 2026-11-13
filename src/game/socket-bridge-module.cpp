// 2026-11-13
// Game-DLL side of resident UDP socket ownership.

#include "game/socket-bridge-module.h"

#include "host/abi/socket-capability.h"

namespace mimita::game {
namespace {
const mimita::host::SocketCapabilityV1* gSocket = nullptr;
void* gHostData = nullptr;
}  // namespace

void initializeHostSocket(const mimita::host::HostServicesV1* services) {
    gSocket = nullptr;
    gHostData = services ? services->hostUserData : nullptr;
    if (!services || !services->get_capability) return;
    auto* capability = static_cast<const mimita::host::SocketCapabilityV1*>(
        services->get_capability(
            services->hostUserData,
            static_cast<std::uint32_t>(mimita::host::HostCapabilityId::Socket),
            mimita::host::kSocketCapabilityVersion));
    if (capability &&
        capability->version == mimita::host::kSocketCapabilityVersion &&
        capability->structSize >= sizeof(mimita::host::SocketCapabilityV1)) {
        gSocket = capability;
    }
}

SOCKET openUdpSocket() {
    if (!gSocket || !gSocket->open_udp) return INVALID_SOCKET;
    const std::uint64_t handle = gSocket->open_udp(gHostData);
    return handle ? static_cast<SOCKET>(handle) : INVALID_SOCKET;
}

void closeSocket(SOCKET socketHandle) {
    if (socketHandle == INVALID_SOCKET) return;
    if (gSocket && gSocket->close) gSocket->close(gHostData, static_cast<std::uint64_t>(socketHandle));
}

bool bindSocket(SOCKET socketHandle, const sockaddr_in* address) {
    return gSocket && gSocket->bind_address && address &&
           gSocket->bind_address(gHostData, static_cast<std::uint64_t>(socketHandle),
                                 address, sizeof(sockaddr_in)) != 0;
}

bool getSocketLocalAddress(SOCKET socketHandle, sockaddr_in* address) {
    return gSocket && gSocket->get_local_address && address &&
           gSocket->get_local_address(gHostData, static_cast<std::uint64_t>(socketHandle),
                                      address, sizeof(sockaddr_in)) != 0;
}

bool setSocketNonBlocking(SOCKET socketHandle) {
    return gSocket && gSocket->set_nonblocking &&
           gSocket->set_nonblocking(gHostData, static_cast<std::uint64_t>(socketHandle)) != 0;
}

bool setSocketReuse(SOCKET socketHandle, bool enabled) {
    return gSocket && gSocket->set_reuse_address &&
           gSocket->set_reuse_address(gHostData, static_cast<std::uint64_t>(socketHandle),
                                      enabled ? 1 : 0) != 0;
}

bool setSocketExclusive(SOCKET socketHandle, bool enabled) {
    return gSocket && gSocket->set_exclusive_address &&
           gSocket->set_exclusive_address(gHostData, static_cast<std::uint64_t>(socketHandle),
                                          enabled ? 1 : 0) != 0;
}

bool setSocketRecvTimeout(SOCKET socketHandle, std::uint32_t milliseconds) {
    return gSocket && gSocket->set_recv_timeout &&
           gSocket->set_recv_timeout(gHostData, static_cast<std::uint64_t>(socketHandle),
                                     milliseconds) != 0;
}

bool sendToSocket(SOCKET socketHandle, const void* data, std::uint32_t size,
                  const sockaddr_in* address) {
    return gSocket && gSocket->send_to && address &&
           gSocket->send_to(gHostData, static_cast<std::uint64_t>(socketHandle),
                            data, size, address, sizeof(sockaddr_in)) != 0;
}

bool recvFromSocket(SOCKET socketHandle, void* data, std::uint32_t capacity,
                    sockaddr_in* address, std::uint32_t* received,
                    bool* wouldBlock) {
    std::int32_t blocked = 0;
    const bool ok = gSocket && gSocket->recv_from &&
        gSocket->recv_from(gHostData, static_cast<std::uint64_t>(socketHandle),
                           data, capacity, address, sizeof(sockaddr_in),
                           received, &blocked) != 0;
    if (wouldBlock) *wouldBlock = blocked != 0;
    return ok;
}
}  // namespace mimita::game
