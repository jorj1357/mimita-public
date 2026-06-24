// Purpose:
// - Immediate downward impulse on Q press
// - No resource, no cooldown, no scripted behavior
// - Collisions naturally handle landing/sliding/bouncing
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

    if (p.freeze.freezeActive)
        return;

    if (!p.dash.downDashAvailable)
        return;

    float beforeVelZ = p.vel.z;

    p.vel.z = DOWN_DASH_SPEED;
    p.dash.downDashAvailable = false;

    DD_LOG(
        "[DOWN_DASH] vel.z %.3f -> %.3f (onGround=%d)\n",
        beforeVelZ,
        p.vel.z,
        (int)p.ground.onGround
    );
}
