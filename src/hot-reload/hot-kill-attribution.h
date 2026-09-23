// 09 23 2026
/* purpose
* Define the generic, hot-replaceable NPC kill-reattribution policy and the ONE
* implementation shared by the cold EXE fallback and the hot provider. When a
* player dies to an ownerless/self-inflicted blow but was recently damaged by an
* NPC, the kill is credited to that NPC within a bounded window. The EXE owns
* kill recording; a hot module owns the window and the decision.
* POD only: no STL or engine objects cross the boundary.
* Does NOT own kill recording or replication.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

namespace MimitaNet {

struct GameKillAttributionV1 {
    std::uint32_t structSize;
    // inputs
    std::uint32_t attackerNpcId;
    std::uint32_t attackerPlayerId;
    std::uint32_t victimId;
    std::uint32_t lastNpcDamageSourceId;
    std::uint32_t lastNpcDamageTick;
    std::uint32_t tick;
    std::uint32_t windowTicks;        // e.g. 120 (2 s at 60 Hz)
    // out
    std::uint32_t outAttackerNpcId;
    std::uint32_t outAttackerPlayerId;
    std::uint32_t reattributed;
    std::uint32_t result;
};

using GameKillAttributionFn = void (MIMITA_GAME_CALL *)(
    void* host, GameKillAttributionV1* request);

static constexpr std::uint64_t GAME_CAP_KILL_ATTRIBUTION =
    gameHash("net.kill-attribution");
static constexpr std::uint64_t GAME_SIG_KILL_ATTRIBUTION =
    gameHash("sig.net.kill-attribution.v1");

namespace HotKillAttributionImpl {

inline void evaluate(GameKillAttributionV1& r)
{
    r.outAttackerNpcId = r.attackerNpcId;
    r.outAttackerPlayerId = r.attackerPlayerId;
    r.reattributed = 0u;
    r.result = 1u;

    const bool hasRealPlayerAttacker =
        r.attackerPlayerId != 0 && r.attackerPlayerId != r.victimId;
    if (r.attackerNpcId == 0 && !hasRealPlayerAttacker &&
        r.lastNpcDamageSourceId != 0 &&
        (r.tick - r.lastNpcDamageTick) <= r.windowTicks)
    {
        r.outAttackerNpcId = r.lastNpcDamageSourceId;
        r.outAttackerPlayerId = 0u;
        r.reattributed = 1u;
    }
}

} // namespace HotKillAttributionImpl

} // namespace MimitaNet
