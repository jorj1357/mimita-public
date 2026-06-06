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

// CHANGED: Removed checkDashDirectionCancel — dash is now a single impulse in vel, jun 6 2026

void doWalk(
    Player& p,
    const glm::vec2& wishMoveXY,
    float dt
) {
    dt = std::min(dt, 0.033f);

    float wishLen = glm::length(wishMoveXY);

    // no movement input
    if (wishLen < 0.0001f)
    {
        // grounded stop should be quick but not destructive
        if (p.stableOnGround)
        {
            p.vel.x *= 0.80f;
            p.vel.y *= 0.80f;

            if (std::abs(p.vel.x) < 0.01f) p.vel.x = 0.0f;
            if (std::abs(p.vel.y) < 0.01f) p.vel.y = 0.0f;
        }

        WALK_LOG("[WALK] No input\n");
        return;
    }

    glm::vec2 wishDir = wishMoveXY / wishLen;
    glm::vec2 velXY(p.vel.x, p.vel.y);

    // ---------------- GROUND ----------------
    if (p.stableOnGround)
    {
        float targetSpeed = PHYS.moveSpeed;

        glm::vec2 targetVel =
            wishDir * targetSpeed;

        float accel = 42.0f;
        float t = std::min(1.0f, accel * dt);

        glm::vec2 newVel =
            glm::mix(velXY, targetVel, t);

        WALK_LOG(
            "[WALK][GROUND] vel(%.2f %.2f) -> (%.2f %.2f)\n",
            velXY.x,
            velXY.y,
            newVel.x,
            newVel.y
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
        float maxAirSpeed =
            PHYS.moveSpeed * PHYS.airGain;

        float speed = glm::length(velXY);

        if (speed < baseSpeed)
            speed = baseSpeed;

        float boostedSpeed =
            std::min(speed * PHYS.airGain, maxAirSpeed);

        glm::vec2 target =
            wishDir * boostedSpeed;

        glm::vec2 newVel =
            glm::mix(velXY, target, t);

        WALK_LOG(
            "[WALK][AIR] vel(%.2f %.2f) -> (%.2f %.2f)\n",
            velXY.x,
            velXY.y,
            newVel.x,
            newVel.y
        );

        p.vel.x = newVel.x;
        p.vel.y = newVel.y;
    }
}