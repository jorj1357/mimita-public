// 09 22 2026
/* purpose
* Resolves the authoritative (JSON- or cpp-selected) gameplay tuning for a
* weapon definition so hot tool behaviors can use the SAME values as the cold
* registry. Backs the generic `weapon.tuning` capability.
* Does NOT own weapons: it only reports what the registry resolved.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

namespace MimitaNet {

// Fills `out` from the registry for the network id. Returns false when the id
// is unknown or no weapon is registered.
bool serverWeaponTuning(std::uint32_t weaponDefNetworkId, GameWeaponTuningV1* out);

} // namespace MimitaNet
