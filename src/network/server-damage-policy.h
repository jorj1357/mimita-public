// 09 12 2026
/* purpose
* Kernel-side authoritative damage policy resolver.
* Builds a generic GAME_EVENT_DAMAGE_POLICY event, lets the hot gameplay
* behavior override the base damage/knockback, and applies the explicit
* separation between gameplay tuning and the kernel safety bound.
* Does NOT own behavior code, replication, or entity state.
*/
#pragma once

#include <cstdint>

#include <glm/glm.hpp>

#include "hot-reload/game-api.h"

struct ServerDamagePolicyInput {
    std::uint32_t source = 0;  // GameDamageSource
    std::uint64_t attackerEntity = 0;
    std::uint64_t victimEntity = 0;
    std::uint64_t projectileEntity = 0;
    std::uint32_t weaponNetworkId = 0;
    std::uint32_t victimIsNpc = 0;
    float distance = 0.0f;
    std::uint64_t tick = 0;
};

// Dispatches the hot damage policy, falls back to baseDamage when unhandled,
// and applies the explicit authoritative safety limit. Writes knockback back.
int serverResolveDamagePolicy(const ServerDamagePolicyInput& input,
                              int baseDamage, glm::vec3& knockback);

// Explicit kernel safety bound, separate from gameplay tuning.
// 0 = unlimited (private development). Public servers set a finite value.
int serverAuthoritativeDamageLimit();
