// 2026-11-13
#pragma once

#include "host/abi/game-host-abi.h"

namespace mimita::game {
void initializeHostGlobals(const mimita::host::HostServicesV1* services);
void syncHostGlobals();
}  // namespace mimita::game
