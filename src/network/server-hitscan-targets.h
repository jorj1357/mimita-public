// 09 22 2026
/* purpose
* Publishes/clears the authoritative rewound actor hitboxes for one hitscan
* trace so the hot hitscan behavior can validate against exactly what the cold
* authority used. Uses only the generic dynamic-component store.
* Does NOT own weapon definitions, damage, ammo, or networking.
*/
#pragma once

#include <cstdint>
#include <vector>

#include "combat/weapon-execution.h"

namespace MimitaNet {

// Writes one HitscanTargetBoxes component per (entity, target) pair. `entities`
// and `targets` are parallel; extra entries are ignored.
void publishHitscanTargets(
    const std::vector<std::uint64_t>& entities,
    const std::vector<WeaponExecution::PlayerTarget>& targets);

// Removes the temporary geometry from every entity in `entities`.
void clearHitscanTargets(const std::vector<std::uint64_t>& entities);

} // namespace MimitaNet
