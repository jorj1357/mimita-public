// 09 23 2026
/* purpose
* Define the generic, hot-replaceable projectile splash falloff policy (damage
* by distance and the knockback scale by distance) and the ONE implementation
* shared by the cold EXE fallback and the hot provider. The EXE owns the
* projectile state, the victim loops, LOS, and the authoritative damage apply; a
* hot module owns the falloff curves.
* POD only: no STL or engine objects cross the boundary.
* Does NOT own projectile state, collision, or damage application.
*/
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "hot-reload/game-api.h"

namespace MimitaNet {

struct GameSplashFalloffV1 {
    std::uint32_t structSize;
    // inputs
    float distance;
    float fullDamageRadius;
    float splashRadius;
    float splashDamage;
    float edgeDamage;
    float splashExponent;
    float damageValue;    // in for the knock scale (the computed damage)
    // out
    float outDamage;
    float outKnockScale;
    std::uint32_t result;
};

using GameSplashDamageFn = void (MIMITA_GAME_CALL *)(void* host,
                                                     GameSplashFalloffV1* request);
using GameSplashKnockScaleFn = void (MIMITA_GAME_CALL *)(void* host,
                                                         GameSplashFalloffV1* request);

struct GameProjectileSplashPolicyV1 {
    std::uint32_t structSize;
    std::uint32_t version;
    GameSplashDamageFn damage;
    GameSplashKnockScaleFn knockScale;
    const char* name;
};

using GameProjectileSplashLookupFn =
    const GameProjectileSplashPolicyV1* (MIMITA_GAME_CALL *)(void* host);

static constexpr std::uint64_t GAME_CAP_PROJECTILE_SPLASH =
    gameHash("net.projectile-splash");
static constexpr std::uint64_t GAME_SIG_PROJECTILE_SPLASH =
    gameHash("sig.net.projectile-splash.v1");

// ── The single shared implementation ────────────────────────────────
namespace HotProjectileSplashImpl {

inline void damage(GameSplashFalloffV1& r)
{
    if (r.fullDamageRadius > 0.0f) {
        if (r.distance <= r.fullDamageRadius) {
            r.outDamage = r.splashDamage;
        } else if (r.distance >= r.splashRadius) {
            r.outDamage = r.edgeDamage;
        } else {
            const float t = (r.distance - r.fullDamageRadius) /
                std::max(0.001f, r.splashRadius - r.fullDamageRadius);
            r.outDamage = r.splashDamage + (r.edgeDamage - r.splashDamage) * t;
        }
    } else {
        r.outDamage = r.splashDamage *
            std::exp(-std::pow(r.distance / r.splashRadius, 2.0f) * r.splashExponent);
    }
    r.result = 1u;
}

inline void knockScale(GameSplashFalloffV1& r)
{
    if (r.fullDamageRadius > 0.0f) {
        r.outKnockScale = std::clamp(
            r.damageValue / std::max(0.001f, r.splashDamage), 0.0f, 1.0f);
    } else {
        const float t = r.distance / r.splashRadius;
        r.outKnockScale = (1.0f - t * t) * 0.85f + 0.15f;
    }
    r.result = 1u;
}

} // namespace HotProjectileSplashImpl

} // namespace MimitaNet
