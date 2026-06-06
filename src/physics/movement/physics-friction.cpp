// C:\important\quiet\n\mimita-priv-v7\src\physics\movement\physics-friction.cpp
// feb 10 2026
/**
 * purpose
 * handles all drag and friction
 * should expose groundfriction(args)
 * and airfriction(args)
 * so that other files can just do like 
 * for example
 * dash logic
 * if dash pressed
 * then dash(direction,velocity)
 * inside that function
 * applyfriction(air, amount)
 * etc
 */

/**
 * mar 8 2026
 * this file should do 
 * 1 single friction value
 * air friction and ground friction are the same, and should both be low 
 * e.g. drag in air is same as drag on ground 
 * so find a nice balance
 * also 
 * either this file or a new file
 * make air strafing ?
 * cant decide between no A + D classic CS air strafe 
 * vs W = forward, strafe forward
 * A + D = gain speed when doing it , but i ehh i dont like it 
 * i want specific functions in the game to let u gain speed like dash 
 * but A + D is simpler easier and understanded wide
 * but W = forward is even simpler and just weird and strange bruh idk
 * for now just do W = forward i dont wanna add A + D strafing
 * just make it so turns u do in the air with ALL directions or ANY of them
 * e.g. hold W = forward, turn left and right super fast = gain hella speed
 * just do that 
 * no A + D bullcrap heh
 */

#include <cstdio>
#include <cmath>
#include <algorithm>
#include <glm/glm.hpp>

#include "physics/config.h"
#include "entities/player.h"
#include "debug/debug-log.h"
#include "config/player-settings.h"

// =====================================================
// DEBUG
// =====================================================
#define FRICTION_LOG(...) Debug::logThrottled(Debug::Category::Physics, "friction", DebugConfig::PRINT_INTERVAL, __VA_ARGS__)

// =====================================================
// HELPERS
// =====================================================

static inline float expDecay(float amount, float dt) {
    return std::exp(-amount * dt);
}

// =====================================================
// FRICTION
// =====================================================

void doFriction(
    Player& p,
    bool onGround,
    float dt
) {
    // --------------------------------------------------
    // BASE VELOCITY FRICTION
    // --------------------------------------------------
    float frictionAmount =
        onGround ? GROUND_FRICTION_AMOUNT : AIR_FRICTION_AMOUNT;

    float decay = expDecay(frictionAmount, dt);

    glm::vec2 velXY(p.vel.x, p.vel.y);
    float speedBefore = glm::length(velXY);

    velXY *= decay;

    // Kill tiny drift
    if (glm::length(velXY) < ALMOST_ZERO) {
        velXY = glm::vec2(0.0f);
    }

    p.vel.x = velXY.x;
    p.vel.y = velXY.y;

    float impulseDecay = expDecay(GetPlayerSettings().weaponRecoilDecay, dt);
    p.externalImpulse *= impulseDecay;
    if (glm::length(p.externalImpulse) < ALMOST_ZERO)
        p.externalImpulse = glm::vec3(0.0f);

    if (speedBefore > ALMOST_ZERO) {
        FRICTION_LOG(
            "[FRICTION][BASE] %s decay=%.3f speed %.3f -> %.3f\n",
            onGround ? "GROUND" : "AIR",
            decay,
            speedBefore,
            glm::length(velXY)
        );
    }

    // --------------------------------------------------
    // DASH DRAG (SEPARATE)
    // --------------------------------------------------
    if (glm::length(p.dashVel) > ALMOST_ZERO) {

        float dashFriction = onGround
            ? GROUND_FRICTION_AMOUNT * 0.75f
            : AIR_FRICTION_AMOUNT * DRAG_FRICTION_MULTIPLIER;
        float dashDecay = expDecay(dashFriction, dt);

        float dashBefore = glm::length(p.dashVel);

        p.dashVel *= dashDecay;

        if (glm::length(p.dashVel) < ALMOST_ZERO) {
            p.dashVel = glm::vec2(0.0f);
        }

        FRICTION_LOG(
            "[FRICTION][DASH] decay=%.3f speed %.3f -> %.3f\n",
            dashDecay,
            dashBefore,
            glm::length(p.dashVel)
        );
    }

    // --------------------------------------------------
    // SAFETY CLAMP (RUNAWAY PREVENTION)
    // --------------------------------------------------
    glm::vec2 totalXY = glm::vec2(p.vel.x, p.vel.y) + p.dashVel;
    float totalSpeed = glm::length(totalXY);

    if (totalSpeed > MAX_PLAYER_MOVE_SPEED) {
        glm::vec2 clamped = (totalXY / totalSpeed) * MAX_PLAYER_MOVE_SPEED;

        // Preserve dash ratio
        // glm::vec2 dashDir =
        //     glm::length(p.dashVel) > ALMOST_ZERO
        //         ? glm::normalize(p.dashVel)
        //         : glm::vec2(0.0f);

        // better version? idk
        glm::vec2 totalXY = glm::vec2(p.vel.x, p.vel.y) + p.dashVel;
        float totalSpeed = glm::length(totalXY);

        if (totalSpeed > MAX_PLAYER_MOVE_SPEED)
        {
            float scale = MAX_PLAYER_MOVE_SPEED / totalSpeed;

            p.vel.x *= scale;
            p.vel.y *= scale;
            p.dashVel *= scale;
        }

        // dont need? idk mar 8 2026 
        // p.dashVel = dashDir * glm::length(p.dashVel);
        p.vel.x = clamped.x - p.dashVel.x;
        p.vel.y = clamped.y - p.dashVel.y;

        FRICTION_LOG(
            "[FRICTION][CLAMP] speed capped to %.2f\n",
            MAX_PLAYER_MOVE_SPEED
        );
    }
}
