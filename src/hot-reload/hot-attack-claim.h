// 09 23 2026
/* purpose
* Define the generic, hot-replaceable client hit-claim eligibility and tolerance
* policy ("shoot what I saw") and the ONE implementation shared by the cold EXE
* fallback and the hot provider. The EXE owns the rewound pose reconstruction,
* the body-part volume test, and the damage apply; a hot module owns whether a
* claim is structurally eligible and the acceptance tolerance.
* POD only: no STL or engine objects cross the boundary.
* Does NOT own hit geometry, poses, or damage.
*/
#pragma once

#include <algorithm>
#include <cstdint>

#include "hot-reload/game-api.h"

namespace MimitaNet {

struct GameAttackClaimV1 {
    std::uint32_t structSize;
    // inputs
    std::uint32_t claimedTargetId;
    std::uint32_t alreadyConfirmed;   // the re-trace already hit the claimed target
    float claimedDistance;
    float maxRange;
    float worldBlockDistance;
    float rewindHitTolerance;
    float claimLagAllowance;
    // out
    std::uint32_t eligible;           // structural gates pass; cold does the volume test
    std::uint32_t occluded;
    float tolerance;
    std::uint32_t result;
};

using GameAttackClaimFn = void (MIMITA_GAME_CALL *)(void* host,
                                                    GameAttackClaimV1* request);

static constexpr std::uint64_t GAME_CAP_ATTACK_CLAIM = gameHash("net.attack-claim");
static constexpr std::uint64_t GAME_SIG_ATTACK_CLAIM =
    gameHash("sig.net.attack-claim.v1");

namespace HotAttackClaimImpl {

inline void evaluate(GameAttackClaimV1& r)
{
    r.result = 1u;
    r.eligible = 0u;
    r.occluded = 0u;
    // Base rewind tolerance plus a lag allowance so a hit that connects on the
    // target's rendered body registers even when motion-filter lag puts the
    // server's rewind pose slightly ahead of what the shooter saw.
    r.tolerance = std::max(0.0f, r.rewindHitTolerance) +
                  std::max(0.0f, r.claimLagAllowance);

    if (r.claimedTargetId == 0u)
        return;
    if (r.alreadyConfirmed)
        return;
    if (!(r.claimedDistance > 0.001f && r.claimedDistance <= r.maxRange))
        return;
    if (r.worldBlockDistance < r.claimedDistance - 0.1f) {
        r.occluded = 1u;
        return;
    }
    r.eligible = 1u;
}

} // namespace HotAttackClaimImpl

} // namespace MimitaNet
