// 2026-11-12
// Game-side CAP_ENTITY helpers.

#pragma once

#include "host/abi/entity-capability.h"
#include "host/abi/game-host-abi.h"

namespace mimita::game {

void initializeHostEntity(const mimita::host::HostServicesV1* services);
mimita::host::HostResult readHostEntity(mimita::host::EntityHandle handle,
                                        mimita::host::EntitySnapshotV1* out);
mimita::host::HostResult writeHostEntity(
    mimita::host::EntityHandle handle,
    const mimita::host::EntitySnapshotV1* in);

}  // namespace mimita::game
