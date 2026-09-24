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
// Reuse the same capability; the cold bridge resolves both callables by id and
// dispatches by struct shape.

static constexpr std::uint64_t GAME_CAP_ATTACK_CLAIM = gameHash("net.attack-claim");
static constexpr std::uint64_t GAME_SIG_ATTACK_CLAIM =
    gameHash("sig.net.attack-claim.v1");

// Given the client-supplied part and a matched template box's canonical part
// (0 = torso, 1 = head, other = limb), resolve the reported claim part. The
// cold geometry does the box test; this decides how a claimed part is recorded.
struct GameClaimPartV1 {
    std::uint32_t structSize;
    std::uint32_t claimedPart;      // client-supplied (0 = unspecified)
    std::uint32_t boxPart;          // 0 torso, 1 head, other limb
    // out
    std::uint32_t resolvedPart;     // 1 head, 2 torso, 3 leg
    std::uint32_t result;
};

using GameClaimPartFn = void (MIMITA_GAME_CALL *)(void* host, GameClaimPartV1* request);

static constexpr std::uint64_t GAME_CAP_CLAIM_PART = gameHash("net.claim-part");
static constexpr std::uint64_t GAME_SIG_CLAIM_PART = gameHash("sig.net.claim-part.v1");

namespace HotAttackClaimImpl {

// Canonical part resolution: a specified head/leg claim is preserved; otherwise
// derive from the matched box (head -> 1, torso -> 2, limb -> 3).
inline void resolvePart(GameClaimPartV1& r)
{
    if (r.claimedPart == 0u || r.claimedPart == 2u) {
        r.resolvedPart = (r.boxPart == 0u) ? 2u : (r.boxPart == 1u ? 1u : 3u);
    } else {
        r.resolvedPart = r.claimedPart;
    }
    r.result = 1u;
}

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
