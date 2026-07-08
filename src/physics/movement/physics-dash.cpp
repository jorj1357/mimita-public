#include <cstdio>
#include <cmath>
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

#include "physics/movement/physics-dash.h"
#include "physics/config.h"
#include "entities/player.h"
#include "debug/debug-log.h"
#include "effects/effect-part.h"

#define DASH_LOG(...) Debug::logThrottled(Debug::Category::Physics, "dash", DebugConfig::PRINT_INTERVAL, __VA_ARGS__)

// static constexpr float TICK_DT = 1.0f / 60.0f;
// 3x more lenient 7 8 2026 
static constexpr float TICK_DT = 1.0f / 20.0f;

void doAirDash(
    Player& p,
    const glm::vec2& wishMoveXY,
    bool jumpTriggered,
    bool movementPressed,
    bool airborne,
    int movementTicks,
    float movementHeldDuration,
    float dt,
    const glm::vec3& camForward
) {
    (void)dt;
    (void)movementPressed;

    if (!jumpTriggered) return;
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

    // Determine quality: tick-perfect (< 1 tick hold) overrides everything
    bool tickPerfect = movementHeldDuration < TICK_DT;
    float mult;
    if (tickPerfect) {
        mult = dashQualityMultiplier(DashQuality::Perfect);
        p.dash.tickPerfectDash = true;
        p.dash.frictionOverride = 0.0f;

        // Tick perfect popup
        EffectPart e;
        e.position = p.pos + glm::vec3(0.0f, 0.0f, 2.0f);
        e.color = {0.2f, 1.0f, 1.0f};
        e.velocity = {0.0f, 0.0f, 3.0f};
        e.maxLifetime = 0.5f;
        e.label = "TICK PERFECT";
        e.replayType = "damage_number";
        e.billboardText = true;
        e.scale = 0.8f;
        e.endScale = 0.0f;
        e.alpha = 1.0f;
        EffectPartSystem::instance().spawn(e);

    } else {
        DashQuality quality = dashQualityFromTicks(movementTicks);
        mult = dashQualityMultiplier(quality);
        p.dash.lastDashQuality = (int)quality;
        p.dash.frictionOverride = 1.0f;
    }
    float impulse = AIR_DASH_IMPULSE * mult;

    p.vel.x += dashDir.x * impulse;
    p.vel.y += dashDir.y * impulse;

    p.dash.dashAvailable = false;
    p.dash.didDash = true;
    p.jump.airJumpsLeft = 0;

    DASH_LOG(
        "[AIR_DASH] dir=(%.2f %.2f) impulse=%.1f mult=%.2f tickPerfect=%d holdDuration=%.4f\n",
        dashDir.x, dashDir.y, impulse, mult, (int)tickPerfect, movementHeldDuration
    );
    if (tickPerfect)
        Debug::log(Debug::Category::Physics, "[TICK PERFECT DASH] tick=%d holdDuration=%.4f frictionOverride=%.2f\n",
                   (int)(glfwGetTime() * 60.0), movementHeldDuration, p.dash.frictionOverride);
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
