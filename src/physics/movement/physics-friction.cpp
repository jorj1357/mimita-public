// Friction now only handles external impulse decay.
// Velocity friction (ground + air) is owned by doWalk.

#include <cmath>
#include <algorithm>
#include <glm/glm.hpp>

#include "physics/config.h"
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
    (void)onGround;
    dt = std::min(dt, 0.033f);

    // External impulse decays independently of walk friction.
    float impulseDecay = expDecay(EXTERNAL_IMPULSE_DECAY, dt);
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

    FRICTION_LOG(
        "[FRICTION] impulse=%.3f\n",
        glm::length(p.externalImpulse)
    );
}
