// 2026-11-13
// Shared PACKET_SERVER_COMMAND policy for the probe generation and the
// whole-game generation. The host owns the response endpoint; this writes only
// the bounded response bytes.

#pragma once

#include "host/abi/game-host-abi.h"
#include "host/abi/transport-capability.h"

namespace mimita::game {

bool dispatchServerCommand(mimita::host::TransportDispatchV1* dispatch,
                           const mimita::host::HostServicesV1* services);

}  // namespace mimita::game
