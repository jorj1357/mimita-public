// 09 23 2026
/* purpose
* Define the generic, hot-replaceable authoritative damage-application policy
* (accept/reject, friendly-fire filter, authoritative damage clamp, and the
* death/respawn rule) and the ONE implementation shared by the cold EXE fallback
* and the hot provider. The EXE owns health/velocity mutation, movement impulse
* recording, kill recording, and replication; a hot module owns the rules.
* POD only: no STL, ServerPlayer, or engine objects cross the boundary.
* Does NOT own health storage, transport, or kill recording.
*/
#pragma once

#include <algorithm>
#include <cstdint>

#include "hot-reload/game-api.h"

namespace MimitaNet {

// Rejection reasons (numeric values are the cross-boundary contract).
enum GameDamageRejectV1 : std::uint32_t {
    GAME_DAMAGE_REJECT_NONE = 0,
    GAME_DAMAGE_REJECT_TARGET_DEAD = 1,
    GAME_DAMAGE_REJECT_NON_POSITIVE = 2,
    GAME_DAMAGE_REJECT_TARGET_RECONNECTING = 3,
    GAME_DAMAGE_REJECT_FRIENDLY_FIRE = 4,
};

struct GameDamageApplicationV1 {
    std::uint32_t structSize;

    // inputs
    std::uint32_t targetDead;
    std::uint32_t targetConnectionStale;
    std::uint32_t attackerIsTarget;   // self-damage always allowed
    std::uint32_t attackerFound;
    std::int32_t  targetTeam;         // < 0 = no team
    std::int32_t  attackerTeam;
    std::int32_t  damage;
    std::int32_t  damageLimit;        // serverAuthoritativeDamageLimit(); 0 = none
    std::uint32_t respawnsEnabled;
    float         respawnSeconds;     // serverMatchRespawnSeconds()
    std::int32_t  targetHealth;

    // outputs
    std::uint32_t accept;             // 1 = apply damage
    std::uint32_t rejectReason;       // GameDamageRejectV1
    std::int32_t  clampedDamage;
    std::uint32_t killed;             // 1 = health reaches 0
    std::int32_t  healthAfter;
    float         outRespawnSeconds;  // respawn rule applied on death
    std::uint32_t result;             // 1 = the policy produced a decision
};

using GameDamageApplicationFn = void (MIMITA_GAME_CALL *)(
    void* host, GameDamageApplicationV1* request);

static constexpr std::uint64_t GAME_CAP_DAMAGE_APPLICATION =
    gameHash("net.damage-application");
static constexpr std::uint64_t GAME_SIG_DAMAGE_APPLICATION =
    gameHash("sig.net.damage-application.v1");

// ── The single shared implementation ────────────────────────────────
namespace HotDamageApplicationImpl {

inline void evaluate(GameDamageApplicationV1& r)
{
    r.accept = 0u;
    r.rejectReason = GAME_DAMAGE_REJECT_NONE;
    r.clampedDamage = 0;
    r.killed = 0u;
    r.healthAfter = r.targetHealth;
    r.outRespawnSeconds = r.respawnSeconds;
    r.result = 1u;

    if (r.targetDead || r.damage <= 0) {
        r.rejectReason = r.targetDead ? GAME_DAMAGE_REJECT_TARGET_DEAD
                                      : GAME_DAMAGE_REJECT_NON_POSITIVE;
        return;
    }
    // Disconnected/reconnecting players take no damage.
    if (r.targetConnectionStale) {
        r.rejectReason = GAME_DAMAGE_REJECT_TARGET_RECONNECTING;
        return;
    }
    // Team-based friendly fire filtering; self-damage always allowed.
    if (!r.attackerIsTarget && r.targetTeam >= 0) {
        if (r.attackerFound && r.attackerTeam >= 0 &&
            r.attackerTeam == r.targetTeam) {
            r.rejectReason = GAME_DAMAGE_REJECT_FRIENDLY_FIRE;
            return;
        }
    }

    std::int32_t clamped = std::max(1, r.damage);
    if (r.damageLimit > 0 && clamped > r.damageLimit)
        clamped = r.damageLimit;
    r.accept = 1u;
    r.clampedDamage = clamped;
    r.healthAfter = std::max(0, r.targetHealth - clamped);
    if (r.healthAfter == 0) {
        r.killed = 1u;
        r.outRespawnSeconds = r.respawnsEnabled ? r.respawnSeconds : -1.0f;
    }
}

} // namespace HotDamageApplicationImpl

} // namespace MimitaNet
