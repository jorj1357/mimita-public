// C:\important\quiet\n\mimita-priv-v7\src\physics\movement\physics-friction.cpp

#include <cstdio>
#include <cmath>
#include <algorithm>
#include <glm/glm.hpp>

#include "physics/config.h"
#include "entities/player.h"
#include "debug/debug-log.h"
#include "config/player-settings.h"

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
    if (!(onGround && hasMoveInput))
    {
        float frictionAmount =
            onGround
            ? GROUND_FRICTION_AMOUNT
            : AIR_FRICTION_AMOUNT;

        float decay = expDecay(frictionAmount, dt);
        velXY *= decay;

        if (glm::length(velXY) < ALMOST_ZERO)
            velXY = glm::vec2(0.0f);

        p.vel.x = velXY.x;
        p.vel.y = velXY.y;
    }

    // external impulses still decay
    float impulseDecay =
        expDecay(GetPlayerSettings().weaponRecoilDecay, dt);

    p.externalImpulse *= impulseDecay;

    if (glm::length(p.externalImpulse) < ALMOST_ZERO)
        p.externalImpulse = glm::vec3(0.0f);

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

    glm::vec2 xy(p.vel.x, p.vel.y);
    float totalSpeed = glm::length(xy);

    if (totalSpeed > MAX_PLAYER_MOVE_SPEED)
    {
        glm::vec2 clamped =
            (xy / totalSpeed) * MAX_PLAYER_MOVE_SPEED;

        p.vel.x = clamped.x;
        p.vel.y = clamped.y;
    }
}