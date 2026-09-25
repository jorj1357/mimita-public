// 09 24 2026
/* purpose
* Define the generic, hot-replaceable NPC navigation-goal policy and the ONE
* implementation shared by the cold EXE fallback and the hot provider. The EXE
* owns pathfinding, steering, and world queries; the hot policy owns the mapping
* from brain state + bounded memory to an abstract navigation goal
* (reach/follow/maintain/flee). Migration Phase 5f.
* POD only: no STL or engine objects cross the boundary.
* Does NOT own pathfinding, steering, collision, the loop, or movement physics.
*/
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "hot-reload/game-api.h"

namespace MimitaNet {

namespace HotNpcNavGoalImpl {

// Mirror of NpcGoalKind numeric values (src/npc/npc-goal.h).
enum : std::uint32_t {
    kNone = 0,
    kReachPosition = 1,
    kFollowActor = 2,
    kMaintainDistance = 3,
    kFleeActor = 4,
    kReachLineOfSight = 5,
};

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

inline float planarDistance(const float a[3], const float b[3])
{
    const float dx = a[0] - b[0];
    const float dy = a[1] - b[1];
    const float dz = a[2] - b[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

inline void copyPos(const float src[3], float dst[3])
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

inline void evaluate(NpcNavGoalPolicyV1& r)
{
    r.handled = 1u;
    r.result = 1u;
    r.goalKind = kNone;
    r.goalTargetPos[0] = 0.0f;
    r.goalTargetPos[1] = 0.0f;
    r.goalTargetPos[2] = 0.0f;
    r.goalDesiredDistance = 0.0f;

    if (!r.hasTarget) {
        if (r.currentState == kChase) {
            r.goalKind = kReachPosition;
            copyPos(r.lastKnownTarget, r.goalTargetPos);
        } else if (r.lastAttackerAge < 6.0f &&
                   planarDistance(r.lastAttackerPos, r.selfPos) > 2.0f) {
            r.goalKind = kReachPosition;
            copyPos(r.lastAttackerPos, r.goalTargetPos);
        } else if (r.recentDangerAge < 6.0f &&
                   planarDistance(r.recentDangerPos, r.selfPos) > 2.0f) {
            r.goalKind = kReachPosition;
            copyPos(r.recentDangerPos, r.goalTargetPos);
        } else if (r.currentState == kRandomWalk) {
            r.goalKind = kReachPosition;
            copyPos(r.wanderTarget, r.goalTargetPos);
        }
        return;
    }

    switch (r.currentState) {
    case kChase:
    case kAdvance:
        r.goalKind = kFollowActor;
        break;
    case kCircle:
    case kStrafe:
    case kHoldPosition:
    case kPeek:
    case kAim:
        r.goalKind = kMaintainDistance;
        r.goalDesiredDistance = r.preferredRange > 0.0f
            ? r.preferredRange
            : std::clamp(r.effectiveRange * 0.6f, 5.0f, 25.0f);
        r.goalDesiredDistance *= r.maintainDistanceScale;
        break;
    case kRetreat:
    case kRecover:
        r.goalKind = kFleeActor;
        r.goalDesiredDistance = 8.0f;
        break;
    default:
        r.goalKind = kFollowActor;
        break;
    }
}

} // namespace HotNpcNavGoalImpl

} // namespace MimitaNet
