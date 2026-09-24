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
static constexpr std::uint32_t HOT_PROJECTILE_BOUNCE_ON_WORLD = 8u;

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
    float restitution;         // bounce energy retention (0..1+)
    std::uint64_t ownerEntity;
    std::uint64_t toolEntity;
    std::uint64_t typeId;      // package projectile type key (presentation/impact)
    std::uint32_t flags;       // HOT_PROJECTILE_*
    std::uint32_t maxBounces;
    std::uint32_t bounces;
    // Append-only single-detonation guard. Once a projectile has detonated or
    // begun destruction, `detonated` is set and the system refuses to process
    // another explosion for this entity. This makes "spawn -> simulate -> first
    // contact -> explode once -> destroy" authoritative even when the entity is
    // observed again before the destroy is visible. `explosionReason` records
    // the first cause (0 none, 1 world, 2 actor, 3 lifetime, 4 direct).
    std::uint32_t detonated;
    std::uint32_t explosionReason;
    // Append-only fire correlation for logging (owner's fire serial, weapon id).
    std::uint64_t fireSerial;
    std::uint64_t weaponNetworkId;
};

// HotProjectileStateV1::explosionReason values (hot-only; logging/diagnostics).
static constexpr std::uint32_t HOT_PROJECTILE_REASON_NONE = 0u;
static constexpr std::uint32_t HOT_PROJECTILE_REASON_WORLD = 1u;
static constexpr std::uint32_t HOT_PROJECTILE_REASON_ACTOR = 2u;
static constexpr std::uint32_t HOT_PROJECTILE_REASON_LIFETIME = 3u;
static constexpr std::uint32_t HOT_PROJECTILE_REASON_DIRECT = 4u;
