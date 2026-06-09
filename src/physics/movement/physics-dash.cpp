// C:\important\quiet\n\mimita-priv-v7\src\physics\movement\physics-dash.cpp
// feb 10 2026
//
// Purpose:
// - Handle ONLY dash logic
// - Config-based (physics/config.h)
// - No audio
// - No collisions
// - Heavy debug logging (but guarded)

// CHANGED: Dash is now a single-frame additive external impulse, jun 6 2026
// Removed: dashVel, dashLockedDirection, cancelDashVelocity
// Subsequent frames preserve, steer, and slowly decay that momentum.

#include <cstdio>
#include <cmath>
#include <glm/glm.hpp>

#include "physics/config.h"
#include "entities/player.h"
#include "debug/debug-log.h"

// =====================================================
// DEBUG TOGGLE
// =====================================================
#define DASH_LOG(...) Debug::logThrottled(Debug::Category::Physics, "dash", DebugConfig::PRINT_INTERVAL, __VA_ARGS__)

// =====================================================
// DASH
// =====================================================

// =====================================================
// AIR DASH
// =====================================================

void doAirDash(
    Player& p,
    const glm::vec2& wishMoveXY,
    bool jumpPressed,
    bool movementPressed,
    bool airborne,
    float dt
) {
    (void)dt;

    if (!jumpPressed) return;
    if (!airborne) return;
    if (!movementPressed) return;
    if (!p.dashAvailable) return;
    if (p.freezeActive) return;

    glm::vec2 wishLen = wishMoveXY;
    if (glm::length(wishLen) < 0.001f)
        return;

    glm::vec2 dashDir = glm::normalize(wishLen);

    // weaker directional impulse applied directly to vel
    p.vel.x += dashDir.x * AIR_DASH_IMPULSE;
    p.vel.y += dashDir.y * AIR_DASH_IMPULSE;

    p.dashAvailable = false;
    p.didDash = true;

    // prevent air jump on same frame
    p.airJumpsLeft = 0;

    DASH_LOG(
        "[AIR_DASH] dir=(%.2f %.2f) impulse=%.1f vel=(%.2f %.2f)\n",
        dashDir.x, dashDir.y, AIR_DASH_IMPULSE,
        p.vel.x, p.vel.y
    );
}

void doDash(
    Player& p,
    const glm::vec2& wishMoveXY,
    bool dashPressed,
    const glm::vec3& camForward,
    float dt
) {
    if (!dashPressed)
        return;

    if (!p.dashAvailable)
    {
        DASH_LOG("[DASH][FAIL] dash already used\n");
        return;
    }

    if (p.freezeActive)
        return;

    // --------------------------------------------------
    // DASH DIRECTION
    // --------------------------------------------------

    glm::vec2 dashDir = wishMoveXY;

    if (glm::length(dashDir) < 0.001f)
        dashDir = glm::normalize(glm::vec2(camForward.x, camForward.y));
    else
        dashDir = glm::normalize(dashDir);

    // --------------------------------------------------
    // SINGLE-FRAME ADDITIVE IMPULSE
    // --------------------------------------------------

    glm::vec2 impulse =
    dashDir * DASH_IMPULSE;

    // add dash as EXTERNAL physics force
    p.externalImpulse.x += impulse.x;
    p.externalImpulse.y += impulse.y;

    // clamp ONLY external impulse
    glm::vec2 impulseXY(
        p.externalImpulse.x,
        p.externalImpulse.y
    );

    float impulseSpeed =
        glm::length(impulseXY);

    float maxImpulseSpeed =
        MAX_EXTERNAL_IMPULSE_SPEED;

    if (impulseSpeed > maxImpulseSpeed)
    {
        glm::vec2 clamped =
            (impulseXY / impulseSpeed) *
            maxImpulseSpeed;

        p.externalImpulse.x = clamped.x;
        p.externalImpulse.y = clamped.y;
    }
    
    // consume dash
    p.dashAvailable = false;

    // trigger effects
    p.didDash = true;

    DASH_LOG(
        "[DASH]\n"
        " dir=(%.2f %.2f)\n"
        " impulse=(%.2f %.2f)\n",
        dashDir.x, dashDir.y,
        impulse.x, impulse.y
    );
}
