// Unified movement: instant ground + momentum-preserving air
//
// Ground:
//   1. Instant velocity snap to moveSpeed in wish direction
//   2. Friction when no input (decelerate to stop)
//   3. Skip when jumpHeld — preserves air velocity for bhop
//
// Air:
//   1. CS-style additive acceleration in wish direction
//   2. Never clamps total speed — dash momentum survives air
//   3. Zero friction (coast freely)
//
// Future: slide, wall-run, wall-jump, grapple all plug in here
//   by adding flags (isSliding, isWallRunning) that adjust how
//   velocity is set vs accumulated.

#include <cstdio>
#include <algorithm>
#include <cmath>

#include "physics/config.h"
#include "entities/player.h"
#include "debug/debug-log.h"

#define WALK_LOG(...) Debug::logThrottled(Debug::Category::Physics, "walk", DebugConfig::PRINT_INTERVAL, __VA_ARGS__)

// Ground friction: speed-proportional deceleration used when no input.
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

// CS-style air acceleration: adds velocity toward wishdir, capped per frame.
// Never clamps total speed — preserves dash, bhop, and external momentum.
static void applyAirAccelerate(glm::vec2& velXY, const glm::vec2& wishDir, float dt)
{
    float currentSpeed = glm::dot(velXY, wishDir);
    float addSpeed = PHYS.moveSpeed - currentSpeed;
    if (addSpeed <= 0.0f)
        return;

    float accelSpeed = AIR_ACCEL_AMOUNT * PHYS.moveSpeed * dt;
    addSpeed = std::min(addSpeed, accelSpeed);
    velXY += wishDir * addSpeed;
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
    // Skip when jumpHeld is true: preserves air velocity for bhop.
    // The 80ms stableOnGround hysteresis absorbs seams and collision jitter
    // without killing bhop speed.
    if (p.stableOnGround && !jumpHeld)
    {
        if (wishLen > 0.001f)
        {
            // Instant acceleration: velocity immediately matches wish direction at moveSpeed.
            // Future: slide mode would replace this with slide physics.
            glm::vec2 wishDir = wishMoveXY / wishLen;
            steerExternalImpulse(p, wishDir, dt);
            velXY = wishDir * PHYS.moveSpeed;
        }
        else
        {
            // No input: decelerate to stop.
            applyGroundFriction(velXY, dt);
        }

        p.vel.x = velXY.x;
        p.vel.y = velXY.y;

        WALK_LOG("[WALK][GROUND] speed=%.2f jumpHeld=%d\n", glm::length(velXY), (int)jumpHeld);
        return;
    }

    // ---- AIR (or bhop ground skip) ----
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
        // No input in air: coast (no friction).
        // Future: air drag / glide would go here.
        if (glm::length(glm::vec2(p.externalImpulse.x, p.externalImpulse.y)) > 0.001f)
        {
            glm::vec2 impulseDir = glm::normalize(glm::vec2(p.externalImpulse.x, p.externalImpulse.y));
            steerExternalImpulse(p, impulseDir, dt);
        }
    }
}
