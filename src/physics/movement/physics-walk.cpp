// 07 21 2026, 10 54
/* purpose
* Adapts current Player walking to the shared Stage 2A walk helper.
* Preserves instant horizontal velocity replacement, size scaling, and diagnostics.
* Leaves current physics ordering and Player-specific logging in the legacy wrapper.
* Does NOT apply gravity, jump, friction, dash, freeze, or collision.
* Does NOT poll input, send packets, render, play audio, or decide authority.
* Does NOT replace the complete movement orchestrator.
*/

#include <cstdio>
#include <algorithm>
#include <cmath>

#include "physics/config.h"
#include "config/size-scaling-config.h"
#include "physics/movement/movement-step.h"
#include "entities/player.h"
#include "debug/debug-log.h"

// =====================================================
// DEBUG
// =====================================================

#define WALK_LOG(...) Debug::logThrottled(Debug::Category::Physics, "walk", DebugConfig::PRINT_INTERVAL, __VA_ARGS__)

// =====================================================
// WALK
// =====================================================

// CHANGED: Removed checkDashDirectionCancel — dash is now a single impulse in vel, jun 6 2026

void doWalk(
    Player& p,
    const glm::vec2& wishMoveXY,
    bool onGround,
    float dt
) {
    dt = std::min(dt, 0.033f);

    float wishLen = glm::length(wishMoveXY);

    // no movement input — friction (handled separately) will stop the player
    if (wishLen < 0.0001f)
    {
        WALK_LOG("[WALK] No input\n");
        return;
    }

    glm::vec2 wishDir = wishMoveXY / wishLen;
    glm::vec2 velBefore(p.vel.x, p.vel.y);

    // Set horizontal velocity to max speed along wish direction.
    // Instant direction change. No additive accumulation.
    const auto& sc = SizeScalingConfig::instance().data();
    float maxSpeed = (onGround ? PHYS.moveSpeed : AIR_SPEED) *
                     movementSizeScaleFactor(p.sizeScale, sc.movementSpeedExponent);
    movementApplyWalkVelocity(
        p.vel, wishMoveXY, onGround, p.sizeScale, PHYS.moveSpeed, AIR_SPEED,
        sc.movementSpeedExponent);

    // Debug: air control summary (0.5s throttle)
    if (!onGround)
    {
        Debug::logThrottled(Debug::Category::Physics, "airwalk", 0.5f,
            "[AIR_WALK] grounded=%d wishDir=(%.2f %.2f) velBefore=(%.2f %.2f) velAfter=(%.2f %.2f)\n",
            (int)onGround, wishDir.x, wishDir.y,
            velBefore.x, velBefore.y, p.vel.x, p.vel.y);
    }

    WALK_LOG(
        "[WALK] speed=%.2f maxSpeed=%.2f wish=(%.2f %.2f)\n",
        glm::length(glm::vec2(p.vel.x, p.vel.y)),
        maxSpeed, wishDir.x, wishDir.y
    );
}
