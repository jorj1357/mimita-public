// 09 24 2026
/* purpose
* Define the generic, hot-replaceable NPC target-selection policy and the ONE
* implementation shared by the cold EXE fallback and the hot provider. The EXE
* enumerates candidates (it owns actor storage, teams, and weapon threat data);
* the hot policy owns scored aggregation, current-target stickiness, the
* anti-thrash switch threshold, and the legacy nearest-hostile fallback. This is
* migration Phase 5b: target selection is behavior, not mechanism.
* POD only: no STL or engine objects cross the boundary.
* Does NOT own candidate enumeration, combat, the loop, transport, or damage.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

namespace MimitaNet {

namespace HotNpcTargetSelectImpl {

inline void evaluate(NpcTargetSelectPolicyV1& r)
{
    r.handled = 1u;
    r.result = 1u;
    r.chosenId = 0;
    r.chosenKind = 0;
    if (r.candidateCount == 0)
        return;

    const std::uint32_t count =
        r.candidateCount < (std::uint32_t)NPC_TARGET_SELECT_MAX_CANDIDATES
            ? r.candidateCount
            : (std::uint32_t)NPC_TARGET_SELECT_MAX_CANDIDATES;

    if (r.behaviorActive) {
        float bestScore = -1e30f;
        int bestIdx = -1;
        float currentScore = -1e30f;
        for (std::uint32_t i = 0; i < count; ++i) {
            const NpcTargetCandidateV1& c = r.candidates[i];
            if (!c.alive)
                continue;
            float s = c.score;
            if (c.isCurrent)
                s += r.currentStickiness;
            if (s > bestScore) {
                bestScore = s;
                bestIdx = static_cast<int>(i);
            }
            if (c.isCurrent)
                currentScore = s;
        }
        int chosenIdx = bestIdx;
        // Switch only when a new candidate beats the current target by the
        // configured threshold (reduces target thrashing).
        if (bestIdx >= 0 && r.currentTargetId != 0 && currentScore > -1e29f) {
            if (r.candidates[bestIdx].id != r.currentTargetId &&
                bestScore <= currentScore + r.targetSwitchThreshold) {
                for (std::uint32_t i = 0; i < count; ++i) {
                    if (r.candidates[i].alive && r.candidates[i].isCurrent) {
                        chosenIdx = static_cast<int>(i);
                        break;
                    }
                }
            }
        }
        if (chosenIdx >= 0) {
            r.chosenId = r.candidates[chosenIdx].id;
            r.chosenKind = r.candidates[chosenIdx].kind;
        }
    } else {
        // Legacy nearest-hostile. Strict `<` keeps the first (players before
        // npcs) on ties, matching the cold iteration order.
        float bestD2 = 3.402823466e+38f;
        int bestIdx = -1;
        for (std::uint32_t i = 0; i < count; ++i) {
            const NpcTargetCandidateV1& c = r.candidates[i];
            if (!c.alive)
                continue;
            if (c.distanceSq < bestD2) {
                bestD2 = c.distanceSq;
                bestIdx = static_cast<int>(i);
            }
        }
        if (bestIdx >= 0) {
            r.chosenId = r.candidates[bestIdx].id;
            r.chosenKind = r.candidates[bestIdx].kind;
        }
    }
}

} // namespace HotNpcTargetSelectImpl

} // namespace MimitaNet
