// 07 21 2026, 10 54
/* purpose
* Adapts current Player friction and external impulse decay to shared helpers.
* Preserves base X/Y friction, friction override, impulse decay, and impulse clamp.
* Keeps Player-specific diagnostics outside the neutral Stage 2A movement kernel.
* Does NOT apply walking, gravity, jump, collision, dash, down dash, or freeze.
* Does NOT send packets, play audio, render, or decide movement authority.
* Does NOT own projectile, weapon, damage, or contact reset behavior.
*/

#include <cstdio>
#include <cmath>
#include <algorithm>
#include <glm/glm.hpp>

#include "physics/config.h"
#include "physics/movement/movement-step.h"
#include "entities/player.h"
#include "debug/debug-log.h"

#define FRICTION_LOG(...) Debug::logThrottled(Debug::Category::Physics, "friction", DebugConfig::PRINT_INTERVAL, __VA_ARGS__)

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
        velXY = movementApplyBaseFrictionXY(
            velXY, onGround, hasMoveInput, p.sizeScale, frictionMul,
            GROUND_FRICTION_AMOUNT, AIR_FRICTION_AMOUNT, -0.5f,
            ALMOST_ZERO, dt);
        p.vel.x = velXY.x;
        p.vel.y = velXY.y;
    }

    // External momentum fades slowly and independently of walk friction.
    // Apply friction override so tick-perfect preserves external impulse too.
    movementDecayAndClampExternalImpulse(
        p.externalImpulse, EXTERNAL_IMPULSE_DECAY, frictionMul,
        MAX_EXTERNAL_IMPULSE_SPEED, ALMOST_ZERO, dt);

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
