// C:\important\quiet\n\mimita-priv-v7\src\physics\movement\physics-walk.cpp
// feb 10 2026
/**
 * purpose
 * all logic for walking goes here
 * literallt just
 * wasd = forward back left right
 * exposes walk(direction, velocity) to other files 
 */

//
// Purpose:
// - Ground walk (instant response)
// - Air movement (instant response, camera-relative)
//
// Uses physics/config.h
// Debug heavy
// No friction, no dash, no collisions

#include <cstdio>
#include <algorithm>
#include <cmath>

#include "physics/config.h"
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

    // Set velocity to target speed along wish direction.
    // Preserves perpendicular velocity (dash, knockback, recoil).
    float targetSpeed = PHYS.moveSpeed;
    float alongCurrent = glm::dot(velBefore, wishDir);
    if (alongCurrent < targetSpeed)
    {
        p.vel.x += (targetSpeed - alongCurrent) * wishDir.x;
        p.vel.y += (targetSpeed - alongCurrent) * wishDir.y;
    }

    // Debug: air control summary (0.5s throttle)
    if (!onGround)
    {
        Debug::logThrottled(Debug::Category::Physics, "airwalk", 0.5f,
            "[AIR_WALK] grounded=%d wishDir=(%.2f %.2f) velBefore=(%.2f %.2f) velAfter=(%.2f %.2f)\n",
            (int)onGround, wishDir.x, wishDir.y,
            velBefore.x, velBefore.y, p.vel.x, p.vel.y);
    }

    WALK_LOG(
        "[WALK] speed=%.2f along=%.2f target=%.2f\n",
        glm::length(glm::vec2(p.vel.x, p.vel.y)),
        alongCurrent, targetSpeed
    );
}
