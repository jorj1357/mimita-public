// 2026-11-12
// Purpose: game-DLL transport bridge for CAP_TRANSPORT.
//
// The game generation builds a packet and asks the host to send it. The host
// owns the transport and session; this bridge owns no sockets or session state.

#pragma once

#include <cstddef>
#include <cstdint>

#include "host/abi/game-host-abi.h"
#include "host/abi/transport-capability.h"

namespace mimita::game {

void initializeHostTransport(const mimita::host::HostServicesV1* services);
bool sendToServer(std::uint32_t typeId, const void* data, std::size_t size);
bool sendToPeer(std::uint64_t peer, std::uint32_t typeId, const void* data,
                std::size_t size);
bool bindSession(std::uint64_t peer, std::uint32_t playerId);
bool sendServerCommand(const char* command, std::size_t size);
bool sendServerCommandResult(std::uint32_t playerId, std::int32_t accepted,
                             const char* status);
bool sendToPlayer(std::uint32_t playerId, std::uint32_t typeId,
                  const void* data, std::size_t size);
bool sendToEndpoint(std::uint32_t addressNetworkOrder,
                    std::uint16_t portNetworkOrder, std::uint32_t typeId,
                    const void* data, std::size_t size);
// Opaque resident transport/session. `session` is a host-owned handle; the
// game generation never dereferences it.
bool sendToSession(std::uint64_t session, std::uint32_t typeId,
                   const void* data, std::size_t size);
void closeSession(std::uint64_t session);
bool sessionIsServerHost();
bool queryTransportSwapState(mimita::host::TransportSwapStateV1* out);

}  // namespace mimita::game
