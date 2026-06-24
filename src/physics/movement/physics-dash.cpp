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
    if (!p.dash.dashAvailable) return;
    if (p.freeze.freezeActive) return;

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

    p.dash.dashAvailable = false;
    p.dash.didDash = true;
    p.dash.lastDashQuality = (int)quality;
    p.jump.airJumpsLeft = 0;

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
    if (!dashPressed)
        return;

    if (!p.dash.dashAvailable)
    {
        DASH_LOG("[DASH][FAIL] dash already used\n");
        return;
    }

    if (p.freeze.freezeActive)
        return;

    glm::vec2 dashDir = wishMoveXY;

    if (glm::length(dashDir) < 0.001f)
        dashDir = glm::normalize(glm::vec2(camForward.x, camForward.y));
    else
        dashDir = glm::normalize(dashDir);

    glm::vec2 impulse = dashDir * DASH_IMPULSE;

    p.externalImpulse.x += impulse.x;
    p.externalImpulse.y += impulse.y;

    glm::vec2 impulseXY(p.externalImpulse.x, p.externalImpulse.y);
    float impulseSpeed = glm::length(impulseXY);
    float maxImpulseSpeed = MAX_EXTERNAL_IMPULSE_SPEED;

    if (impulseSpeed > maxImpulseSpeed)
    {
        glm::vec2 clamped = (impulseXY / impulseSpeed) * maxImpulseSpeed;
        p.externalImpulse.x = clamped.x;
        p.externalImpulse.y = clamped.y;
    }

    p.dash.dashAvailable = false;
    p.dash.didDash = true;

    DASH_LOG(
        "[DASH] dir=(%.2f %.2f) impulse=(%.2f %.2f)\n",
        dashDir.x, dashDir.y, impulse.x, impulse.y
    );
}
