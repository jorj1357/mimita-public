// 09 23 2026
/* purpose
* Define the generic, hot-replaceable physical-contact damage/knockback policy
* and the ONE implementation shared by the cold EXE fallback and the hot
* provider. The EXE owns the weapon definition, the contact shape/hit, the
* authoritative damage apply, and the episode batching; a hot module owns the
* per-behavior damage and knockback formulas.
* POD only: no STL, WeaponDefinition, or engine objects cross the boundary.
* Does NOT own contact detection, transport, or authoritative state.
*/
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "hot-reload/game-api.h"

namespace MimitaNet {

// Stable behavior kind the cold bridge maps from WeaponBehaviorType.
enum GamePhysicalContactKindV1 : std::uint32_t {
    GAME_PHYSICAL_KIND_GODBALL = 0,
    GAME_PHYSICAL_KIND_QUICKHIT = 1,
    GAME_PHYSICAL_KIND_SWORD = 2,
};

struct GamePhysicalContactDamageV1 {
    std::uint32_t structSize;
    std::uint32_t kind;               // GamePhysicalContactKindV1
    std::uint32_t swordLunge;
    std::uint32_t shapeHasLength;     // length(currentB - currentA) > 0.001
    float dt;
    float shapeTravelDistance;
    // resolved params (cold)
    float baseDamage;
    float speedDamageFactor;
    float maxDamageCap;
    float forceDamageScale;
    float forceDamageExponent;
    float minDamage;
    float maxDamage;
    // out
    std::int32_t outDamage;
    std::uint32_t result;
};

struct GamePhysicalContactKnockbackV1 {
    std::uint32_t structSize;
    std::uint32_t kind;
    std::uint32_t swordLunge;
    std::int32_t damage;
    float normal[3];
    // resolved params (cold)
    float forceKnockbackScale;
    float maxKnockback;
    float minKnockback;
    float swordKnockback;             // resolved lunge/slash knockback (sword)
    // out
    float outKnockback[3];
    std::uint32_t result;
};

using GamePhysicalContactDamageFn = void (MIMITA_GAME_CALL *)(
    void* host, GamePhysicalContactDamageV1* request);
using GamePhysicalContactKnockbackFn = void (MIMITA_GAME_CALL *)(
    void* host, GamePhysicalContactKnockbackV1* request);
using GamePhysicalContactIntervalFn = std::uint32_t (MIMITA_GAME_CALL *)(
    void* host, float seconds, float tickRate);
using GamePhysicalContactConfirmFn = std::uint32_t (MIMITA_GAME_CALL *)(
    void* host, std::uint32_t active, std::int32_t pendingDamage,
    std::uint32_t ending, std::uint32_t samples, std::uint32_t batchSize);

struct GamePhysicalContactPolicyV1 {
    std::uint32_t structSize;
    std::uint32_t version;
    GamePhysicalContactDamageFn damage;
    GamePhysicalContactKnockbackFn knockback;
    GamePhysicalContactIntervalFn intervalTicks;
    GamePhysicalContactConfirmFn shouldConfirm;
    const char* name;
};

using GamePhysicalContactLookupFn =
    const GamePhysicalContactPolicyV1* (MIMITA_GAME_CALL *)(void* host);

static constexpr std::uint64_t GAME_CAP_PHYSICAL_CONTACT =
    gameHash("net.physical-contact");
static constexpr std::uint64_t GAME_SIG_PHYSICAL_CONTACT =
    gameHash("sig.net.physical-contact.v1");

// ── The single shared implementation ────────────────────────────────
namespace HotPhysicalContactImpl {

inline std::int32_t damage(GamePhysicalContactDamageV1& r)
{
    const float safeDt = std::max(r.dt, 0.0001f);
    if (r.kind == GAME_PHYSICAL_KIND_GODBALL) {
        const float speed = r.shapeTravelDistance / safeDt;
        return std::clamp(
            (std::int32_t)std::round(r.baseDamage + speed * r.speedDamageFactor), 1,
            (std::int32_t)std::max(1.0f, r.maxDamageCap));
    }
    if (r.kind == GAME_PHYSICAL_KIND_QUICKHIT) {
        const float speed = r.shapeTravelDistance / safeDt;
        const float directness = r.shapeHasLength ? 0.8f : 1.0f;
        const float rawForce = speed * directness;
        const float value =
            r.minDamage + std::pow(rawForce * r.forceDamageScale, r.forceDamageExponent);
        return std::clamp((std::int32_t)std::round(value),
                          (std::int32_t)std::max(1.0f, r.minDamage),
                          (std::int32_t)std::max(1.0f, r.maxDamage));
    }
    return std::clamp((std::int32_t)std::round(r.baseDamage), 1, 500);
}

inline void knockback(GamePhysicalContactKnockbackV1& r)
{
    float nx = r.normal[0], ny = r.normal[1], nz = r.normal[2];
    float len = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (len < 0.001f) {
        nx = 0.0f; ny = 0.0f; nz = 1.0f;
    }
    nz = std::max(nz, 0.15f);
    len = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (len < 0.001f) {
        nx = 0.0f; ny = 0.0f; nz = 1.0f;
    } else {
        nx /= len; ny /= len; nz /= len;
    }

    float strength;
    if (r.kind == GAME_PHYSICAL_KIND_QUICKHIT) {
        strength = std::clamp((float)r.damage * r.forceKnockbackScale,
                              r.minKnockback, r.maxKnockback);
    } else {
        strength = std::max(1.0f, r.damage * 0.75f);
        if (r.kind != GAME_PHYSICAL_KIND_GODBALL)
            strength = r.swordKnockback;
    }
    r.outKnockback[0] = nx * strength;
    r.outKnockback[1] = ny * strength;
    r.outKnockback[2] = nz * strength;
}

// Seconds -> whole ticks, minimum one.
inline std::uint32_t intervalTicks(float seconds, float tickRate)
{
    if (seconds <= 0.0f)
        return 1u;
    return std::max<std::uint32_t>(1u, (std::uint32_t)std::ceil(seconds * tickRate));
}

// Episode confirm batching: never confirm an empty/inactive episode; confirm
// immediately when the episode ends; otherwise once enough samples accumulated.
inline std::uint32_t shouldConfirm(std::uint32_t active, std::int32_t pendingDamage,
                                   std::uint32_t ending, std::uint32_t samples,
                                   std::uint32_t batchSize)
{
    if (!active || pendingDamage <= 0)
        return 0u;
    if (ending)
        return 1u;
    return samples >= std::max<std::uint32_t>(1u, batchSize) ? 1u : 0u;
}

} // namespace HotPhysicalContactImpl

} // namespace MimitaNet
