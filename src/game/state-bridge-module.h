// 2026-11-13
#pragma once

#include <cstddef>
#include <cstdint>

#include "host/abi/game-host-abi.h"
#include "host/abi/state-capability.h"

namespace mimita::game {
void initializeHostState(const mimita::host::HostServicesV1* services);
mimita::host::MenuStateV1* menuState();
void* ensureResidentObject(mimita::host::StateObjectId id, std::size_t size,
                           std::size_t alignment, bool* created);
}  // namespace mimita::game
