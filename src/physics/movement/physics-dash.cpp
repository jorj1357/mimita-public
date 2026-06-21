#include <cstdio>
#include <cmath>
#include <glm/glm.hpp>

#include "physics/movement/physics-dash.h"
#include "physics/config.h"
#include "entities/player.h"
#include "debug/debug-log.h"

#define DASH_LOG(...) Debug::logThrottled(Debug::Category::Physics, "dash", DebugConfig::PRINT_INTERVAL, __VA_ARGS__)

void doAirDash(
    Player& p,
    const glm::vec2& wishMoveXY,
    bool triggerPressed,
    bool movementPressed,
    bool airborne,
    int movementTicks,
    float dt,
    const glm::vec3& camForward
) {
    (void)dt;
    (void)movementPressed;

    if (!triggerPressed) return;
    if (!airborne) return;
    if (!p.dashAvailable) return;
    if (p.freezeActive) return;

    glm::vec2 dashDir = wishMoveXY;

    // If movement keys are held, use camera-relative WASD direction.
    // Otherwise, use horizontal camera forward as fallback.
    if (glm::length(dashDir) < 0.001f) {
        dashDir = glm::vec2(camForward.x, camForward.y);
        if (glm::length(dashDir) < 0.001f)
            return;
    }

    dashDir = glm::normalize(dashDir);

    DashQuality quality = dashQualityFromTicks(movementTicks);
    float mult = dashQualityMultiplier(quality);
    float impulse = AIR_DASH_IMPULSE * mult;

    p.vel.x += dashDir.x * impulse;
    p.vel.y += dashDir.y * impulse;

    p.dashAvailable = false;
    p.didDash = true;
    p.lastDashQuality = (int)quality;
    p.airJumpsLeft = 0;

    DASH_LOG(
        "[AIR_DASH] dir=(%.2f %.2f) impulse=%.1f mult=%.2f quality=%d ticks=%d\n",
        dashDir.x, dashDir.y, impulse, mult, (int)quality, movementTicks
    );
}

void doDash(
    Player& p,
    const glm::vec2& wishMoveXY,
    bool dashPressed,
    const glm::vec3& camForward,
    float dt
) {
    (void)dt;

    if (!dashPressed)
        return;

    if (!p.dashAvailable)
    {
        DASH_LOG("[DASH][FAIL] dash already used\n");
        return;
    }

    if (p.freezeActive)
        return;

    glm::vec2 dashDir = wishMoveXY;

    if (glm::length(dashDir) < 0.001f)
    {
        dashDir = glm::vec2(camForward.x, camForward.y);
        if (glm::length(dashDir) < 0.001f)
            return;
        dashDir = glm::normalize(dashDir);
    }
    else
    {
        dashDir = glm::normalize(dashDir);
    }

    glm::vec2 impulse = dashDir * DASH_IMPULSE;

    p.vel.x += impulse.x;
    p.vel.y += impulse.y;

    glm::vec2 velXY(p.vel.x, p.vel.y);
    float speed = glm::length(velXY);
    float maxSpeed = MAX_PLAYER_MOVE_SPEED;

    if (speed > maxSpeed)
    {
        glm::vec2 clamped = (velXY / speed) * maxSpeed;
        p.vel.x = clamped.x;
        p.vel.y = clamped.y;
    }

    p.dashAvailable = false;
    p.didDash = true;

    DASH_LOG(
        "[DASH] dir=(%.2f %.2f) impulse=(%.2f %.2f)\n",
        dashDir.x, dashDir.y, impulse.x, impulse.y
    );
}
