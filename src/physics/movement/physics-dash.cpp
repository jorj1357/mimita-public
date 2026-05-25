// C:\important\quiet\n\mimita-priv-v7\src\physics\movement\physics-dash.cpp
// feb 10 2026
//
// Purpose:
// - Handle ONLY dash logic
// - Config-based (physics/config.h)
// - No audio
// - No collisions
// - Heavy debug logging (but guarded)

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
    // --------------------------------------------------
    // PLAYER INPUT CANCELS DASH (NEW INPUT ONLY)
    // --------------------------------------------------

    static bool moveHeldPrev = false;

    bool moveHeld = glm::length(wishMoveXY) > 0.01f;
    bool moveJustPressed = moveHeld && !moveHeldPrev;

    moveHeldPrev = moveHeld;

    // only cancel dash if a NEW movement press occurs
    if (moveJustPressed && glm::length(p.dashVel) > 0.0f)
    {
        p.dashVel = glm::vec2(0.0f);

        DASH_LOG("[DASH] cancelled by new movement input\n");
    }
    // --------------------------------------------------
    // EDGE DETECT DASH PRESS
    // --------------------------------------------------

    bool dashJustPressed = dashPressed && !p.dashHeldPrev;
    p.dashHeldPrev = dashPressed;

    if (!dashJustPressed)
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
    // SPEED BASE
    // --------------------------------------------------

    glm::vec2 curVel = glm::vec2(p.vel.x, p.vel.y) + p.dashVel;

    float curSpeed = glm::length(curVel);

    if (curSpeed < 0.01f)
        curSpeed = PHYS.moveSpeed;

    // stronger dash (you said 2x)
    float dashStrength = DASH_IMPULSE;

    // glm::vec2 dashBoost = dashDir * curSpeed * dashStrength;
    // maybe too much with current speed? 
    glm::vec2 dashBoost = dashDir * dashStrength;

    // override dash velocity instead of stacking
    // or... mar 8 2026... stack it on with +=
    // ill test them both, override or add
    // it sucks just override dont add 
    p.dashVel = dashBoost;

    // clamp speed
    float dashSpeed = glm::length(p.dashVel);

    if (dashSpeed > MAX_PLAYER_MOVE_SPEED)
    {
        p.dashVel = (p.dashVel / dashSpeed) * MAX_PLAYER_MOVE_SPEED;
    }

    // consume dash
    p.dashAvailable = false;

    // trigger effects
    p.didDash = true;

    DASH_LOG(
        "[DASH]\n"
        " dir=(%.2f %.2f)\n"
        " dashVel=(%.2f %.2f)\n",
        dashDir.x, dashDir.y,
        p.dashVel.x, p.dashVel.y
    );
}
