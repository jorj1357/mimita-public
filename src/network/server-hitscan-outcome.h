// 09 22 2026
/* purpose
* Owns the authoritative consequences of a hitscan trace: per-victim damage
* policy resolution, damage application, DamageConfirmed/NPC-damage events,
* kill recording, and shot-visual broadcasts.
* Extracted verbatim from the cold hitscan branch so BOTH the cold trace path
* and the hot `hitscan.resolve` capability run the exact same consequence code —
* one consequence owner. This is what lets hot own the trace without dropping
* the authoritative side effects.
* Does NOT compute the trace (pellets/targets/damage/knockback) or own packets.
*/
#pragma once

#include <cstdint>
#include <unordered_map>

#include <glm/glm.hpp>

#include "network/server.h"
#include "combat/weapon-execution.h"

namespace MimitaNet {

// Applies damage + events + shot visuals for a completed trace and returns the
// hit verdict (HIT_VERDICT_*) for the caller's AttackResult.
std::uint8_t serverResolveHitscanOutcome(
    SOCKET sock,
    std::unordered_map<std::uint32_t, ServerPlayer>& players,
    std::unordered_map<std::uint32_t, ServerNpc>& npcs,
    const ServerPlayer& shooter,
    const WeaponDefinition& def,
    const WeaponExecution::HitscanTraceResult& trace,
    const glm::vec3& origin,
    const glm::vec3& direction,
    const glm::vec3& worldHit,
    const glm::vec3& worldNormal,
    float maxRange,
    float worldBlockDistance,
    std::uint32_t requestId,
    std::uint32_t clientSimulationTick,
    std::uint32_t claimedTargetId,
    std::uint32_t tick,
    std::uint64_t& totalPacketsOut);

} // namespace MimitaNet
