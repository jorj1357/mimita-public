// CS-style ground movement + air acceleration
//
// Ground:
//   1. Linear friction (speed-proportional drop)
//   2. Accelerate toward wishdir up to moveSpeed
//
// Air:
//   1. Accelerate in wishdir using CS air acceleration
//      (addspeed = maxspeed - dot(vel, wishdir), clamped by accel)
//   2. Clamp total horizontal speed to AIR_SPEED_CAP
//
// Dash external impulse steering preserved.

#include <cstdio>
#include <algorithm>
#include <cmath>

#include "physics/config.h"
#include "entities/player.h"
#include "debug/debug-log.h"

#define WALK_LOG(...) Debug::logThrottled(Debug::Category::Physics, "walk", DebugConfig::PRINT_INTERVAL, __VA_ARGS__)

static void applyGroundFriction(glm::vec2& velXY, float dt)
{
    float speed = glm::length(velXY);
    if (speed > 0.1f)
    {
        float drop = speed * GROUND_FRICTION_AMOUNT * dt;
        float newSpeed = std::max(0.0f, speed - drop);
        velXY *= newSpeed / speed;
    }
}

static void applyGroundAccelerate(glm::vec2& velXY, const glm::vec2& wishDir, float dt)
{
    float currentSpeed = glm::dot(velXY, wishDir);
    float addSpeed = PHYS.moveSpeed - currentSpeed;
    if (addSpeed <= 0.0f)
        return;

    float accelSpeed = GROUND_ACCELERATE * PHYS.moveSpeed * dt;
    addSpeed = std::min(addSpeed, accelSpeed);
    velXY += wishDir * addSpeed;
}

static void applyAirAccelerate(glm::vec2& velXY, const glm::vec2& wishDir, float dt)
{
    float currentSpeed = glm::dot(velXY, wishDir);
    float addSpeed = PHYS.moveSpeed - currentSpeed;
    if (addSpeed <= 0.0f)
        return;

    float accelSpeed = AIR_ACCEL_AMOUNT * PHYS.moveSpeed * dt;
    addSpeed = std::min(addSpeed, accelSpeed);
    velXY += wishDir * addSpeed;

    float speed = glm::length(velXY);
    if (speed > AIR_SPEED_CAP)
        velXY *= AIR_SPEED_CAP / speed;
}

static void steerExternalImpulse(Player& p, const glm::vec2& wishDir, float dt)
{
    glm::vec2 impulseXY(p.externalImpulse.x, p.externalImpulse.y);
    float impulseSpeed = glm::length(impulseXY);
    if (impulseSpeed <= 0.001f)
        return;

    glm::vec2 impulseDir = impulseXY / impulseSpeed;
    float alignment = glm::dot(wishDir, impulseDir);

    float steerT = std::min(1.0f, EXTERNAL_IMPULSE_STEER_RATE * dt);
    glm::vec2 steeredDir = glm::mix(impulseDir, wishDir, steerT);
    if (glm::length(steeredDir) > 0.001f)
        impulseDir = glm::normalize(steeredDir);

    if (alignment < 0.0f)
    {
        float brake = std::exp(-EXTERNAL_IMPULSE_BRAKE_RATE * -alignment * dt);
        impulseSpeed *= brake;
    }

    impulseXY = impulseDir * impulseSpeed;
    p.externalImpulse.x = impulseXY.x;
    p.externalImpulse.y = impulseXY.y;
}

void doWalk(
    Player& p,
    const glm::vec2& wishMoveXY,
    bool jumpHeld,
    float dt
) {
    dt = std::min(dt, 0.033f);

    float wishLen = glm::length(wishMoveXY);
    glm::vec2 velXY(p.vel.x, p.vel.y);

    // ---- GROUND ----
    if (p.stableOnGround)
    {
        // Skip friction when holding jump (bunnyhopping).
        // Ground friction would kill air-earned velocity during the
        // single frame of ground contact or during the 80ms stableOnGround
        // hysteresis window after a jump.
        if (!jumpHeld)
            applyGroundFriction(velXY, dt);

        if (wishLen > 0.001f)
        {
            glm::vec2 wishDir = wishMoveXY / wishLen;
            steerExternalImpulse(p, wishDir, dt);
            applyGroundAccelerate(velXY, wishDir, dt);
        }

        p.vel.x = velXY.x;
        p.vel.y = velXY.y;

        WALK_LOG("[WALK][GROUND] speed=%.2f jumpHeld=%d\n", glm::length(velXY), (int)jumpHeld);
        return;
    }

    // ---- AIR ----
    if (wishLen > 0.001f)
    {
        glm::vec2 wishDir = wishMoveXY / wishLen;
        steerExternalImpulse(p, wishDir, dt);
        applyAirAccelerate(velXY, wishDir, dt);

        p.vel.x = velXY.x;
        p.vel.y = velXY.y;

        WALK_LOG("[WALK][AIR] speed=%.2f\n", glm::length(velXY));
    }
    else
    {
        // no input in air → coast (no friction)
        // still steer external impulse in camera-forward dir if any
        if (glm::length(glm::vec2(p.externalImpulse.x, p.externalImpulse.y)) > 0.001f)
        {
            glm::vec2 impulseDir = glm::normalize(glm::vec2(p.externalImpulse.x, p.externalImpulse.y));
            steerExternalImpulse(p, impulseDir, dt);
        }
    }
}
