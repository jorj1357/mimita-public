// 09 12 2026
/* purpose
* EXE-side bridge to the hot gameplay policy module.
* Passes projectile/explosion state and base params and receives overrides.
* Does NOT own projectile state, authority, or packet flow.
*/
#pragma once

#include "hot-reload/game-api.h"

namespace LiveGameplay {

bool rocketFlight(const RocketFlightStateV1& state,
                  const RocketFlightParamsV1& base,
                  RocketFlightParamsV1& out);

bool explosion(const ExplosionStateV1& state,
               const ExplosionParamsV1& base,
               ExplosionParamsV1& out);

} // namespace LiveGameplay
