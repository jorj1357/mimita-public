// C:\important\quiet\n\mimita-priv-v7\src\physics\movement\physics-friction.cpp

#include <cstdio>
#include <cmath>
#include <algorithm>
#include <glm/glm.hpp>

#include "physics/config.h"
#include "config/size-scaling-config.h"
#include "entities/player.h"
#include "debug/debug-log.h"

#define FRICTION_LOG(...) Debug::logThrottled(Debug::Category::Physics, "friction", DebugConfig::PRINT_INTERVAL, __VA_ARGS__)

static inline float expDecay(float amount, float dt)
{
    return std::exp(-amount * dt);
}

void doFriction(
    Player& p,
    bool onGround,
    float dt
){
    dt = std::min(dt, 0.033f);

    glm::vec2 velXY(p.vel.x, p.vel.y);
    float speedBefore = glm::length(velXY);

    bool hasMoveInput =
        glm::length(p.inputWishMove) > 0.001f;

    // IMPORTANT:
    // If grounded and actively moving, do NOT apply friction.
    // Walk owns movement while WASD is held.
    float frictionMul = std::clamp(p.dash.frictionOverride, 0.0f, 1.0f);

    if (!(onGround && hasMoveInput))
    {
        const auto& sc = SizeScalingConfig::instance().data();
        float s = std::max(p.sizeScale, 0.001f);
        float frictionAmount =
            (onGround
            ? GROUND_FRICTION_AMOUNT
            : AIR_FRICTION_AMOUNT)
            * frictionMul * sc.scale(1.0f, -0.5f, s); // larger = less friction

        float decay = expDecay(frictionAmount, dt);
        velXY *= decay;

        if (glm::length(velXY) < ALMOST_ZERO)
            velXY = glm::vec2(0.0f);

        p.vel.x = velXY.x;
        p.vel.y = velXY.y;
    }

    // External momentum fades slowly and independently of walk friction.
    // Apply friction override so tick-perfect preserves external impulse too.
    float impulseDecay =
        expDecay(EXTERNAL_IMPULSE_DECAY * frictionMul, dt);

    p.externalImpulse *= impulseDecay;

    if (glm::length(p.externalImpulse) < ALMOST_ZERO)
        p.externalImpulse = glm::vec3(0.0f);

    glm::vec2 impulseXY(p.externalImpulse.x, p.externalImpulse.y);
    float impulseSpeed = glm::length(impulseXY);
    if (impulseSpeed > MAX_EXTERNAL_IMPULSE_SPEED)
    {
        impulseXY *= MAX_EXTERNAL_IMPULSE_SPEED / impulseSpeed;
        p.externalImpulse.x = impulseXY.x;
        p.externalImpulse.y = impulseXY.y;
    }

    if (speedBefore > ALMOST_ZERO)
    {
        FRICTION_LOG(
            "[FRICTION] input=%d ground=%d speed %.3f -> %.3f\n",
            (int)hasMoveInput,
            (int)onGround,
            speedBefore,
            glm::length(glm::vec2(p.vel.x, p.vel.y))
        );
    }

    // Tick-perfect velocity trace (rate-limited)
    if (p.dash.frictionOverride < 1.0f) {
        Debug::logThrottled(Debug::Category::Physics, "friction-override", 0.5f,
            "[VEL MODIFY] source=friction tickPerfect=1 frictionMul=%.3f beforeSpeed=%.1f afterSpeed=%.1f delta=%.1f\n",
            frictionMul, speedBefore, glm::length(glm::vec2(p.vel.x, p.vel.y)),
            speedBefore - glm::length(glm::vec2(p.vel.x, p.vel.y)));
    }
}
