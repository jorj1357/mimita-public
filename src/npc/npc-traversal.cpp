// 09 10 2026
/* purpose
* Implements the capability-aware traversal layer.
* Selects walk/jump/drop/dash/dash-jump from the navigator's waypoint info and
* the actor's MovementConfig, keeps a traversal stable across ticks, and emits
* the matching input intent. Failures request a navigator replan and avoid
* repeatedly retrying the same impossible action.
* Does NOT own pathfinding, combat decisions, or physics.
*/

#include "npc/npc-traversal.h"

#include <algorithm>
#include <cmath>

#include "npc/npc.h"
#include "config/movement-config.h"
#include "debug/debug-log.h"

namespace {

constexpr float kArriveXZ = 1.1f;
constexpr float kArriveZ = 1.4f;
constexpr float kRetargetDist = 1.6f;
constexpr float kStepUp = 0.65f;
constexpr float kDropThreshold = 2.0f;     // steeper than this is a real drop
constexpr float kWalkableSlope = 1.0f;     // |dz|/dist walkable on foot
constexpr float kJumpTakeoffDist = 3.0f;
constexpr float kDashMinDistance = 8.0f;
constexpr int kTraversalStuckTicks = 75;   // ~1.25s with no progress
constexpr int kTraversalTimeoutTicks = 360; // 6s hard backstop
constexpr float kFailCooldown = 0.7f;
constexpr float kDashBanSeconds = 1.5f;

} // anonymous namespace

const char* traversalTypeName(TraversalType type)
{
    switch (type) {
        case TraversalType::Walk:     return "walk";
        case TraversalType::Jump:     return "jump";
        case TraversalType::Drop:     return "drop";
        case TraversalType::Dash:     return "dash";
        case TraversalType::DashJump: return "dash_jump";
    }
    return "walk";
}

void NpcTraversalExecutor::reset()
{
    mActive = false;
    mType = TraversalType::Walk;
    mLastType = TraversalType::Walk;
    mTarget = glm::vec3(0.0f);
    mTicks = 0;
    mJumpIssued = false;
    mDashIssued = false;
    mLastDist = 1e18f;
    mNoProgressTicks = 0;
    mFailCooldown = 0.0f;
    mDashBanTimer = 0.0f;
}

TraversalType NpcTraversalExecutor::select(const Npc& npc, const NpcNavResult& nav,
                                           const MovementConfig& cfg, bool& feasible) const
{
    feasible = true;
    const float singleJump = npcMaxJumpHeight(cfg);
    const bool dashReady = cfg.dashEnabled && npc.body.dash.dashAvailable &&
                           mDashBanTimer <= 0.0f;
    const float dz = nav.heightDelta;
    const float dist = nav.distance;

    if (nav.hasGap) {
        if (!dashReady) { feasible = false; return TraversalType::Walk; }
        return dz > kStepUp ? TraversalType::DashJump : TraversalType::Dash;
    }
    // A walkable slope is walked in either direction; only steeper/larger
    // height changes become jump/drop traversals.
    const float slope = nav.distance > 0.01f ? dz / nav.distance : 0.0f;
    if (std::fabs(dz) < kDropThreshold && std::fabs(slope) <= kWalkableSlope) {
        // Long open run: dash if the actor can.
        if (dashReady && dist > kDashMinDistance) return TraversalType::Dash;
        return TraversalType::Walk;
    }
    if (dz > 0.0f) {
        if (dz <= singleJump) return TraversalType::Jump;
        if (dashReady) return TraversalType::DashJump;  // horizontal assist
        feasible = false;
        return TraversalType::Walk;
    }
    return TraversalType::Drop;
}

void NpcTraversalExecutor::begin(const Npc& npc, TraversalType type,
                                 const glm::vec3& target)
{
    mActive = true;
    mType = type;
    mTarget = target;
    mTicks = 0;
    mJumpIssued = false;
    mDashIssued = false;
    mLastDist = 1e18f;
    mNoProgressTicks = 0;
    // Log traversal starts for interesting changes only (non-walk, or a change
    // of traversal type), so consecutive walk waypoints do not flood the log.
    if (type != TraversalType::Walk || type != mLastType) {
        Debug::log(Debug::Category::NpcMovement,
            "[NPC TRAVERSAL] actor=%u type=%s target=(%.1f,%.1f,%.1f)\n",
            npc.id, traversalTypeName(mType), target.x, target.y, target.z);
    }
    mLastType = type;
}

void NpcTraversalExecutor::finish(const Npc& npc, bool completed)
{
    if (mActive && completed && mType != TraversalType::Walk) {
        Debug::log(Debug::Category::NpcMovement,
            "[NPC TRAVERSAL] actor=%u complete type=%s\n",
            npc.id, traversalTypeName(mType));
    }
    mActive = false;
    mJumpIssued = false;
    mDashIssued = false;
}

void NpcTraversalExecutor::fail(Npc& npc, const char* reason)
{
    Debug::log(Debug::Category::NpcMovement,
        "[NPC TRAVERSAL] actor=%u fail type=%s reason=%s\n",
        npc.id, traversalTypeName(mType), reason);
    if (mType == TraversalType::Dash || mType == TraversalType::DashJump)
        mDashBanTimer = kDashBanSeconds;
    npc.navigator.requestRepath();
    mFailCooldown = kFailCooldown;
    mActive = false;
}

NpcTraversalStep NpcTraversalExecutor::update(Npc& npc, const NpcNavResult& nav,
                                              const MovementConfig* movement, float dt)
{
    NpcTraversalStep step;
    if (mFailCooldown > 0.0f) mFailCooldown -= dt;
    if (mDashBanTimer > 0.0f) mDashBanTimer -= dt;

    if (!nav.valid) {
        if (mActive) finish(npc, true);
        return step;
    }

    const MovementConfig& cfg = movement ? *movement
                                         : MovementJsonConfig::instance().config();
    const glm::vec3 target = nav.hasPath ? nav.waypoint : nav.destination;
    const float dx = target.x - npc.body.pos.x;
    const float dy = target.y - npc.body.pos.y;
    const float planar = std::sqrt(dx * dx + dy * dy);
    const float vertical = std::fabs(target.z - npc.body.pos.z);

    // A material waypoint change ends the current traversal (it completed the
    // previous segment); the next tick selects for the new target.
    if (mActive && glm::length(target - mTarget) > kRetargetDist)
        finish(npc, true);

    // Direct destination arrival (no multi-node path left).
    if (!nav.hasPath && planar <= kArriveXZ && vertical <= kArriveZ) {
        if (mActive) finish(npc, true);
        return step;
    }

    if (!mActive && mFailCooldown <= 0.0f) {
        bool feasible = true;
        const TraversalType type = select(npc, nav, cfg, feasible);
        if (!feasible) {
            mType = nav.hasGap ? TraversalType::Dash
                 : (nav.heightDelta > kStepUp ? TraversalType::Jump
                                              : TraversalType::Walk);
            fail(npc, nav.hasGap ? "no_dash" : "no_jump");
            return step;
        }
        begin(npc, type, target);
    }

    if (!mActive) {
        // Cooling down: walk toward the target without special actions.
        if (planar > 0.001f)
            step.direction = glm::vec3(dx / planar, dy / planar, 0.0f);
        return step;
    }

    step.active = true;
    step.type = mType;
    if (planar > 0.001f)
        step.direction = glm::vec3(dx / planar, dy / planar, 0.0f);
    ++mTicks;

    switch (mType) {
        case TraversalType::Walk:
            break;
        case TraversalType::Jump:
            if (!mJumpIssued && npc.sensors.touchFloor && planar <= kJumpTakeoffDist) {
                step.jump = true;
                mJumpIssued = true;
            }
            break;
        case TraversalType::Drop:
            if (cfg.downDashEnabled && !mDashIssued && !npc.sensors.touchFloor) {
                step.downDash = true;
                mDashIssued = true;
            }
            break;
        case TraversalType::Dash:
            if (!mDashIssued && npc.body.dash.dashAvailable) {
                step.dash = true;
                mDashIssued = true;
            }
            break;
        case TraversalType::DashJump:
            if (!mDashIssued && npc.body.dash.dashAvailable) {
                step.dash = true;
                mDashIssued = true;
            }
            if (!mJumpIssued && npc.sensors.touchFloor && planar <= kJumpTakeoffDist) {
                step.jump = true;
                mJumpIssued = true;
            }
            break;
    }

    // Fail when the actor stops making progress (do not retry forever).
    if (planar < mLastDist - 0.02f) {
        mLastDist = planar;
        mNoProgressTicks = 0;
    } else {
        ++mNoProgressTicks;
    }
    if (mNoProgressTicks > kTraversalStuckTicks || mTicks > kTraversalTimeoutTicks) {
        fail(npc, mNoProgressTicks > kTraversalStuckTicks ? "blocked" : "timeout");
        step.active = false;
        return step;
    }
    return step;
}
