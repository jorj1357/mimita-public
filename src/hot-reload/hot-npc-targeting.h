// 09 23 2026
/* purpose
* Define the generic, hot-replaceable NPC targeting policy (hostility rule and
* candidate score) and the ONE implementation shared by the cold EXE fallback
* and the hot provider. The EXE owns the NPC/player stores, the candidate
* loops, the switch threshold application, and the combat; a hot module owns who
* is hostile and how a candidate is scored.
* POD only: no STL, Npc, or engine objects cross the boundary.
* Does NOT own targeting storage, movement, or damage.
*/
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "hot-reload/game-api.h"

namespace MimitaNet {

struct GameNpcHostilityV1 {
    std::uint32_t structSize;
    std::int32_t teamA;
    std::int32_t teamB;
    // out
    std::uint32_t hostile;
    std::uint32_t result;
};

struct GameNpcTargetScoreV1 {
    std::uint32_t structSize;
    // inputs
    float selfPos[3];
    float candidatePos[3];
    std::int32_t candidateHp;
    std::int32_t candidateMaxHp;
    float threat01;
    std::uint32_t isCurrent;
    // behavior weights
    float distanceTargetBias;
    float lowHealthTargetBias;
    float threatBias;
    float stickiness;
    // out
    float score;
    std::uint32_t result;
};

using GameNpcHostilityFn = void (MIMITA_GAME_CALL *)(void* host,
                                                     GameNpcHostilityV1* request);
using GameNpcTargetScoreFn = void (MIMITA_GAME_CALL *)(void* host,
                                                       GameNpcTargetScoreV1* request);

struct GameNpcTargetingPolicyV1 {
    std::uint32_t structSize;
    std::uint32_t version;
    GameNpcHostilityFn hostile;
    GameNpcTargetScoreFn score;
    const char* name;
};

using GameNpcTargetingLookupFn =
    const GameNpcTargetingPolicyV1* (MIMITA_GAME_CALL *)(void* host);

static constexpr std::uint64_t GAME_CAP_NPC_TARGETING = gameHash("net.npc-targeting");
static constexpr std::uint64_t GAME_SIG_NPC_TARGETING =
    gameHash("sig.net.npc-targeting.v1");

// ── The single shared implementation ────────────────────────────────
namespace HotNpcTargetingImpl {

inline void hostile(GameNpcHostilityV1& r)
{
    // No teams (negative) -> free for all; otherwise only a different team.
    r.hostile = (r.teamA < 0 || r.teamB < 0) ? 1u : (r.teamA != r.teamB ? 1u : 0u);
    r.result = 1u;
}

inline void score(GameNpcTargetScoreV1& r)
{
    const float dx = r.candidatePos[0] - r.selfPos[0];
    const float dy = r.candidatePos[1] - r.selfPos[1];
    const float dz = r.candidatePos[2] - r.selfPos[2];
    const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    const float distanceScore = 1.0f / (1.0f + dist);
    const float healthFrac = r.candidateMaxHp > 0
        ? std::clamp((float)r.candidateHp / (float)r.candidateMaxHp, 0.0f, 1.0f)
        : 1.0f;
    const float vulnerability = 1.0f - healthFrac;
    r.score = distanceScore * r.distanceTargetBias +
              vulnerability * r.lowHealthTargetBias +
              r.threat01 * r.threatBias;
    if (r.isCurrent)
        r.score += r.stickiness;
    r.result = 1u;
}

} // namespace HotNpcTargetingImpl

} // namespace MimitaNet
