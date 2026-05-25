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
// - Air steering
// - Momentum preservation
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

void doWalk(
    Player& p,
    const glm::vec2& wishMoveXY,
    float dt
) {
    dt = std::min(dt, 0.033f);

    float wishLen = glm::length(wishMoveXY);

    // No input → do nothing (friction handled elsewhere)
    if (wishLen < 0.0001f) {
        WALK_LOG("[WALK] No input\n");
        return;
    }

    glm::vec2 wishDir = wishMoveXY / wishLen;
    glm::vec2 velXY(p.vel.x, p.vel.y);

    // ---------------- GROUND ----------------
    if (p.onGround) {
        float curSpeed = glm::length(velXY);

        // Preserve momentum if already faster
        float targetSpeed = std::max(curSpeed, PHYS.moveSpeed);

        glm::vec2 newVel = wishDir * targetSpeed;

        WALK_LOG(
            "[WALK][GROUND] vel(%.2f, %.2f) -> (%.2f, %.2f) speed=%.2f\n",
            velXY.x, velXY.y,
            newVel.x, newVel.y,
            targetSpeed
        );

        p.vel.x = newVel.x;
        p.vel.y = newVel.y;
        return;
    }

    // ---------------- AIR ----------------
    {
        float airSteer = AIR_ACCEL_AMOUNT;
        float t = std::min(1.0f, airSteer * dt);

        float baseSpeed = PHYS.moveSpeed;
        // feb 10 2026 todo just define this as 1 value in config.h
        // not a mult thing
        float maxAirSpeed = PHYS.moveSpeed * PHYS.airGain;

        float speed = glm::length(velXY);

        if (speed < baseSpeed) {
            speed = baseSpeed;
        }

        float boostedSpeed =
            std::min(speed * PHYS.airGain, maxAirSpeed);

        glm::vec2 target = wishDir * boostedSpeed;
        glm::vec2 newVel = glm::mix(velXY, target, t);

        WALK_LOG(
            "[WALK][AIR] vel(%.2f, %.2f) -> (%.2f, %.2f) "
            "t=%.3f boosted=%.2f\n",
            velXY.x, velXY.y,
            newVel.x, newVel.y,
            t,
            boostedSpeed
        );

        p.vel.x = newVel.x;
        p.vel.y = newVel.y;
    }
}
