// 2026-11-12
// Purpose: game-DLL transport bridge for CAP_TRANSPORT.
//
// The game generation builds a packet and asks the host to send it. The host
// owns the transport and session; this bridge owns no sockets or session state.

#pragma once

#include <cstddef>
#include <cstdint>

#include "host/abi/game-host-abi.h"

namespace mimita::game {

void initializeHostTransport(const mimita::host::HostServicesV1* services);
bool sendServerCommand(const char* command, std::size_t size);
bool sendServerCommandResult(std::uint32_t playerId, std::int32_t accepted,
                             const char* status);
bool sendToPlayer(std::uint32_t playerId, std::uint32_t typeId,
                  const void* data, std::size_t size);
bool sessionIsServerHost();

}  // namespace mimita::game
