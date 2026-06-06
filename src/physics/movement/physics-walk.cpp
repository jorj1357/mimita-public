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
            // p.vel.x *= 0.80f;
            // p.vel.y *= 0.80f;

            // 6 6 2026 aoaakka 
            // float stopDrag = std::exp(-8.0f * dt);
            float stopDrag = std::exp(-30.0f * dt);

            p.vel.x *= stopDrag;
            p.vel.y *= stopDrag;

            if (std::abs(p.vel.x) < 0.01f) p.vel.x = 0.0f;
            if (std::abs(p.vel.y) < 0.01f) p.vel.y = 0.0f;
        }

        WALK_LOG("[WALK] No input\n");
        return;
    }

    glm::vec2 wishDir = wishMoveXY / wishLen;
    glm::vec2 velXY(p.vel.x, p.vel.y);

    glm::vec2 impulseXY(
        p.externalImpulse.x,
        p.externalImpulse.y
    );
    float impulseSpeed = glm::length(impulseXY);
    if (impulseSpeed > 0.001f)
    {
        glm::vec2 impulseDir = impulseXY / impulseSpeed;
        float alignment = glm::dot(wishDir, impulseDir);

        float steerT =
            std::min(1.0f, EXTERNAL_IMPULSE_STEER_RATE * dt);
        glm::vec2 steeredDir =
            glm::mix(impulseDir, wishDir, steerT);

        if (glm::length(steeredDir) > 0.001f)
            impulseDir = glm::normalize(steeredDir);

        if (alignment < 0.0f)
        {
            float brake =
                std::exp(-EXTERNAL_IMPULSE_BRAKE_RATE * -alignment * dt);
            impulseSpeed *= brake;
        }

        impulseXY = impulseDir * impulseSpeed;
        p.externalImpulse.x = impulseXY.x;
        p.externalImpulse.y = impulseXY.y;
    }

    // ---------------- GROUND ----------------
    // ---------------- GROUND ----------------
    // ---------------- GROUND ----------------
    if (p.stableOnGround)
    {
        float targetSpeed = PHYS.moveSpeed;

        glm::vec2 targetVel =
            wishDir * targetSpeed;

        // very fast response
        float groundResponse = 28.0f;

        float t =
            std::min(1.0f, groundResponse * dt);

        glm::vec2 currentVel = velXY;

        glm::vec2 newVel =
            glm::mix(currentVel, targetVel, t);

        p.vel.x = newVel.x;
        p.vel.y = newVel.y;

        WALK_LOG(
            "[WALK][GROUND] speed=%.2f\n",
            glm::length(newVel)
        );

        return;
    }

    // ---------------- AIR ----------------
    {
        glm::vec2 currentVel = velXY;

        float airSpeed =
            PHYS.moveSpeed;

        glm::vec2 targetVel =
            wishDir * airSpeed;

        float airResponse = 8.0f;

        float t =
            std::min(1.0f, airResponse * dt);

        glm::vec2 newVel =
            glm::mix(currentVel, targetVel, t);

        p.vel.x = newVel.x;
        p.vel.y = newVel.y;

        WALK_LOG(
            "[WALK][AIR] speed=%.2f\n",
            glm::length(newVel)
        );
    }
}
