// 09 12 2026
/* purpose
* EXE-side bridge to the hot gameplay policy module.
* Passes projectile/explosion state and base params and receives overrides.
* Does NOT own projectile state, authority, or packet flow.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

namespace LiveGameplay {

bool rocketFlight(const RocketFlightStateV1& state,
                  const RocketFlightParamsV1& base,
                  RocketFlightParamsV1& out);

// Structured evidence for one applied policy decision. `side` is "server" or
// "client"; `kind` describes the decision. Records the active generation/hash.
void journalPolicy(const char* side, const char* kind,
                   std::uint64_t projectileId, std::uint64_t targetId,
                   float baseSpeed, float outSpeed,
                   float baseDamage, float outDamage);

} // namespace LiveGameplay
