// 09 24 2026
/* purpose
* Define the generic, hot-replaceable NPC AI state-selection policy and the ONE
* implementation shared by the cold EXE fallback and the hot provider. The EXE
* owns the world/navigation query (isStuck), the mind scalars, and weapon range;
* the hot policy owns the state scoring, the randomness, the anti-thrash
* current-state penalty, and the stuck/hit/no-target guards. Migration Phase 5e.
* POD only: no STL or engine objects cross the boundary.
* Does NOT own navigation, movement generation, the loop, or rendering.
*/
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "hot-reload/game-api.h"

namespace MimitaNet {

namespace HotNpcStateSelectImpl {

// Mirror of NpcState numeric values (src/npc/npc-state-machine.h).
enum : std::uint32_t {
    kIdle = 0,
    kRandomWalk = 1,
    kChase = 2,
    kCircle = 3,
    kStrafe = 4,
    kRetreat = 5,
    kAttack = 6,
    kRecover = 7,
    kAdvance = 8,
    kHoldPosition = 9,
    kPeek = 10,
    kAim = 11,
    kZigZag = 12,
};

inline float clamp01(float v)
{
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

inline float random01(std::uint32_t& state)
{
    state = state * 1664525u + 1013904223u;
    return static_cast<float>((state >> 8) & 0x00ffffffu) /
           static_cast<float>(0x01000000u);
}

inline float scoreState(std::uint32_t s, const NpcStateSelectPolicyV1& r)
{
    const float dist = r.distance;
    const float close01 = 1.0f - clamp01((dist - 2.0f) / 16.0f);
    const float far01 = clamp01((dist - 4.0f) / 146.0f);
    const float mid01 = 1.0f - std::fabs(dist - 8.0f) / 12.0f;
    const float longRange01 = clamp01((dist - 20.0f) / 130.0f);
    const float hasTarget = r.hasTarget ? 1.0f : 0.0f;
    const float agg = r.effectiveAggression;
    const float wepRange = r.weaponRange;
    const float idealDist = r.preferredRange > 0.0f
        ? r.preferredRange
        : std::clamp(wepRange * 0.6f, 5.0f, wepRange);
    float rangeMatch =
        1.0f - std::fabs(dist - idealDist) / std::max(wepRange, 20.0f);
    rangeMatch = clamp01(rangeMatch);

    switch (s) {
    case kIdle:
        return (1.0f - hasTarget) * 0.8f;
    case kRandomWalk:
        return (1.0f - hasTarget) * 0.6f + 0.2f;
    case kChase:
        return hasTarget * (0.3f + far01 * 0.6f + (1.0f - close01) * 0.3f * agg) *
               (0.8f + 0.2f * (1.0f - rangeMatch));
    case kCircle:
        return hasTarget * (close01 * 0.5f + mid01 * 0.4f * agg) *
               (0.7f + 0.3f * rangeMatch);
    case kStrafe:
        return hasTarget * (0.15f + mid01 * 0.5f + (1.0f - longRange01) * 0.3f) *
               (0.7f + 0.3f * rangeMatch);
    case kRetreat:
        return hasTarget * (close01 * 0.3f + (1.0f - agg) * 0.2f) + r.retreatBonus;
    case kAttack:
        if (r.attackCooldown > 0.0f)
            return 0.0f;
        return hasTarget * (close01 * 0.8f + mid01 * 0.4f + far01 * 0.15f) *
               (0.5f + 0.5f * rangeMatch);
    case kRecover:
        return 0.0f;
    case kAdvance:
        return hasTarget * (longRange01 * 0.6f * agg + far01 * 0.3f) *
               (0.6f + 0.4f * (1.0f - rangeMatch));
    case kHoldPosition:
        return hasTarget * (mid01 * 0.3f + (1.0f - agg) * 0.15f) *
               (0.6f + 0.4f * rangeMatch);
    case kPeek:
        return hasTarget * (mid01 * 0.25f + close01 * 0.15f) *
               (0.7f + 0.3f * rangeMatch);
    case kAim:
        if (r.attackCooldown > 0.0f)
            return 0.0f;
        return hasTarget * (mid01 * 0.3f + far01 * 0.2f + close01 * 0.1f) *
               (0.5f + 0.5f * rangeMatch);
    case kZigZag:
        return hasTarget * (mid01 * 0.35f * agg + far01 * 0.2f) *
               (0.7f + 0.3f * (1.0f - rangeMatch));
    }
    return 0.0f;
}

inline void evaluate(NpcStateSelectPolicyV1& r)
{
    r.handled = 1u;
    r.result = 1u;
    const float d01 = r.difficulty01;
    const float dist = r.distance;

    if (r.isStuck) {
        r.stuckTimer = std::min(r.stuckTimer + 0.016f, 1.0f);
        if (r.stuckTimer > 0.3f) {
            r.chosenState = random01(r.rngState) < 0.5f ? kChase : kRandomWalk;
            return;
        }
    } else {
        r.stuckTimer = 0.0f;
    }

    if (r.hitReactionTimer > 0.0f && random01(r.rngState) < 0.6f) {
        r.chosenState = kRecover;
        return;
    }

    if (!r.hasTarget) {
        if (r.lastKnownAge < 8.0f && r.lastKnownAge > 0.5f) {
            if (r.distToLastKnown > 2.0f) {
                r.chosenState = kChase;
                return;
            }
            r.chosenState = kCircle;
            return;
        }
        r.chosenState = random01(r.rngState) < 0.4f ? kIdle : kRandomWalk;
        return;
    }

    if (r.currentState == kRetreat) {
        const float maxRetreat = 0.5f + (1.0f - d01) * 2.5f;
        if (r.retreatTimer > maxRetreat) {
            r.chosenState = dist < 8.0f ? kCircle : kChase;
            return;
        }
    }

    struct Candidate {
        std::uint32_t state;
        float score;
    };
    Candidate candidates[10];
    int candidateCount = 0;
    auto add = [&](std::uint32_t s) {
        const float score = scoreState(s, r);
        if (score > 0.01f && candidateCount < 10)
            candidates[candidateCount++] = {s, score};
    };
    add(kChase);
    add(kCircle);
    add(kStrafe);
    add(kRetreat);
    add(kAttack);
    add(kAdvance);
    add(kHoldPosition);
    add(kPeek);
    add(kAim);
    add(kZigZag);

    const float randomness = (0.15f + d01 * 0.20f) * r.randomnessScale;
    for (int i = 0; i < candidateCount; ++i)
        candidates[i].score *= (1.0f - randomness * random01(r.rngState));

    if (r.aggressionTuning > 0.6f && dist > 4.0f) {
        for (int i = 0; i < candidateCount; ++i)
            if (candidates[i].state == kRetreat)
                candidates[i].score *= 0.1f;
    }
    for (int i = 0; i < candidateCount; ++i)
        if (candidates[i].state == r.currentState)
            candidates[i].score *= 0.7f;

    if (candidateCount == 0) {
        r.chosenState = kChase;
        return;
    }
    int bestIdx = 0;
    for (int i = 1; i < candidateCount; ++i)
        if (candidates[i].score > candidates[bestIdx].score)
            bestIdx = i;
    r.chosenState = candidates[bestIdx].state;
}

} // namespace HotNpcStateSelectImpl

} // namespace MimitaNet
