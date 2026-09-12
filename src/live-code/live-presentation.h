// 09 12 2026
/* purpose
* EXE-side bridge to the hot presentation module.
* Passes base damage-number / rocket-trail styles in and receives overrides.
* Does NOT own effect pools, rendering, or config.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

namespace LivePresentation {

// Returns true and fills `out` when the active module produced a style.
bool formatDamage(const DamageNumberStyleV1& base, int damage, std::uint32_t flags,
                  DamageNumberStyleV1& out);

// Returns true and fills `out` when the active module produced trail params.
bool rocketTrail(const RocketTrailStyleV1& base, RocketTrailStyleV1& out);

} // namespace LivePresentation
