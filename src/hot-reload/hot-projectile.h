// 09 14 2026
/* purpose
* Shared hot projectile-entity state. One canonical layout for every hot
* projectile weapon (banana, rocket, grenade, future). Used by tool behaviors
* that spawn a projectile entity and by the canonical projectiles.60 system that
* simulates it. Hot-only header: not a GameAPI context field.
* Does NOT link into the EXE.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

static constexpr std::uint64_t HOT_PROJECTILE_COMPONENT =
    gameHash("HotProjectileState");

static constexpr std::uint32_t HOT_PROJECTILE_EXPLODE_ON_WORLD = 1u;
static constexpr std::uint32_t HOT_PROJECTILE_EXPLODE_ON_ACTOR = 2u;
static constexpr std::uint32_t HOT_PROJECTILE_EXPLODE_ON_LIFETIME = 4u;

struct HotProjectileStateV1 {
    float position[3];
    float velocity[3];
    float gravity;
    float drag;
    float lifetime;
    float age;
    float radius;
    float impactDamage;        // direct damage on contact
    float splashRadius;        // 0 = direct only
    float splashDamage;
    float splashExponent;
    float knockbackStrength;
    float selfDamageMultiplier;
    float fullDamageRadius;    // inside this radius, full splash damage
    std::uint64_t ownerEntity;
    std::uint64_t toolEntity;
    std::uint64_t typeId;      // package projectile type key (presentation/impact)
    std::uint32_t flags;       // HOT_PROJECTILE_EXPLODE_*
    std::uint32_t reserved;
};
