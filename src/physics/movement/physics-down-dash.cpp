// Purpose:
// - Immediate downward impulse on Q press
// - Additive velocity: does not overwrite existing momentum
// - Collisions naturally handle landing/sliding/bouncing
//
// Future: chained down-dash, stomp-on-hit, ground-pound all
//   add velocity here instead of creating separate systems.
//
// Uses physics/config.h
// No collision / no grounding logic

#include <cstdio>
#include <algorithm>

#include "physics/config.h"
#include "entities/player.h"
#include "debug/debug-log.h"

#define DD_LOG(...) Debug::logThrottled(Debug::Category::Physics, "down-dash", DebugConfig::PRINT_INTERVAL, __VA_ARGS__)

void doDownDash(
    Player& p,
    bool downDashPressed,
    float dt
) {
    (void)dt;

    if (!downDashPressed)
        return;

    if (p.freezeActive)
        return;

    if (!p.downDashAvailable)
        return;

    float beforeVelZ = p.vel.z;

    // Additive: preserves existing upward/downward momentum.
    p.vel.z += DOWN_DASH_SPEED;
    p.downDashAvailable = false;

    DD_LOG(
        "[DOWN_DASH] vel.z %.3f -> %.3f (onGround=%d)\n",
        beforeVelZ,
        p.vel.z,
        (int)p.onGround
    );
}
