// C:\important\quiet\n\mimita-priv-v7\src\physics\movement\physics-dash.cpp
// feb 10 2026
//
// Purpose:
// - Handle ONLY dash logic
// - Config-based (physics/config.h)
// - No audio
// - No collisions
// - Heavy debug logging (but guarded)

// CHANGED: Dash is now a single-frame impulse on vel, jun 6 2026
// Removed: dashVel, dashLockedDirection, cancelDashVelocity
// Dash adds impulse directly to p.vel once on press frame.
// Subsequent frames: friction + walk naturally take over.

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
    // SINGLE-FRAME IMPULSE to vel
    // --------------------------------------------------

    glm::vec2 impulse = dashDir * DASH_IMPULSE;

    p.vel.x += impulse.x;
    p.vel.y += impulse.y;

    // Clamp total horizontal speed
    glm::vec2 totalXY(p.vel.x, p.vel.y);
    float totalSpeed = glm::length(totalXY);
    if (totalSpeed > MAX_PLAYER_MOVE_SPEED)
    {
        glm::vec2 clamped = (totalXY / totalSpeed) * MAX_PLAYER_MOVE_SPEED;
        p.vel.x = clamped.x;
        p.vel.y = clamped.y;
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
