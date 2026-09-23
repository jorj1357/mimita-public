// 09 23 2026
/* purpose
* Define the generic, hot-replaceable server attack-gate policy (cooldown grace,
* per-tick shot rate limit, and hitscan origin geometry tolerance) and the ONE
* implementation shared by the cold EXE fallback and the hot provider. The EXE
* owns the packet, the weapon runtime, the authoritative trace, and the result
* packet; a hot module owns the gate thresholds and decisions.
* POD only: no STL, ServerPlayer, or engine objects cross the boundary.
* Does NOT own weapon state, transport, or damage.
*/
#pragma once

#include <cmath>
#include <cstdint>

#include "hot-reload/game-api.h"

namespace MimitaNet {

struct GameAttackGatesV1 {
    std::uint32_t structSize;

    // inputs
    std::uint32_t hotStateOwns;       // a hot tool-state instance owns cooldown
    std::uint32_t isPhysicalContact;
    std::uint32_t isHitscan;
    std::uint64_t tick;
    std::uint64_t nextAllowedFireTick;
    std::uint64_t cooldownGraceTicks;
    std::uint32_t shotsThisTick;
    std::uint32_t maxShotsPerTick;
    float shooterPos[3];
    float origin[3];
    float direction[3];
    float pingMs;

    // outputs
    std::uint32_t cooldownReject;
    std::uint32_t rateLimitReject;
    std::uint32_t geometryReject;
    float originTolerance;
    std::uint32_t result;
};

using GameAttackGatesFn = void (MIMITA_GAME_CALL *)(void* host,
                                                    GameAttackGatesV1* request);

static constexpr std::uint64_t GAME_CAP_ATTACK_GATES = gameHash("net.attack-gates");
static constexpr std::uint64_t GAME_SIG_ATTACK_GATES =
    gameHash("sig.net.attack-gates.v1");

// ── The single shared implementation ────────────────────────────────
namespace HotAttackGatesImpl {

inline void evaluate(GameAttackGatesV1& r)
{
    r.result = 1u;
    r.cooldownReject = 0u;
    r.rateLimitReject = 0u;
    r.geometryReject = 0u;

    // Muzzle may lead the server's shooter position by the distance the player
    // could travel during their round-trip latency.
    r.originTolerance = 12.0f + r.pingMs / 1000.0f * 200.0f;

    if (!r.hotStateOwns && !r.isPhysicalContact &&
        r.tick + r.cooldownGraceTicks < r.nextAllowedFireTick)
        r.cooldownReject = 1u;

    if (r.isHitscan && r.shotsThisTick >= r.maxShotsPerTick)
        r.rateLimitReject = 1u;

    const bool finiteOrigin = std::isfinite(r.origin[0]) &&
        std::isfinite(r.origin[1]) && std::isfinite(r.origin[2]);
    const float dirLen = std::sqrt(r.direction[0] * r.direction[0] +
                                   r.direction[1] * r.direction[1] +
                                   r.direction[2] * r.direction[2]);
    const float dx = r.origin[0] - r.shooterPos[0];
    const float dy = r.origin[1] - r.shooterPos[1];
    const float dz = r.origin[2] - r.shooterPos[2];
    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (!finiteOrigin || dirLen <= 0.0001f || distance > r.originTolerance)
        r.geometryReject = 1u;
}

} // namespace HotAttackGatesImpl

} // namespace MimitaNet
