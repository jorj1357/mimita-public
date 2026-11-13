// 2026-11-13
#pragma once

#include "host/abi/game-host-abi.h"

namespace MimitaNet { struct ListenServerState; }

namespace mimita::game {
void initializeHostTransportSession(const mimita::host::HostServicesV1* services);
MimitaNet::ListenServerState* hostListenServerState();
}  // namespace mimita::game
