// 07 21 2026, 10 54
/* purpose
* Adapts current Player gravity to the shared Stage 2A gravity helper.
* Preserves local vertical acceleration, dt clamp, fall clamp, and diagnostics.
* Keeps Player-specific logging outside the neutral shared movement kernel.
* Does NOT handle collision, grounding, jump, dash, freeze, walking, or friction.
* Does NOT decide network authority or mutate packet/server state.
* Does NOT launch or own any runtime test harness.
*/

#include <cstdio>
#include <algorithm>

#include "physics/config.h"
#include "physics/movement/movement-step.h"
#include "entities/player.h"
#include "debug/debug-log.h"

// =====================================================
// DEBUG
// =====================================================

#define GRAV_LOG(...) Debug::logThrottled(Debug::Category::Physics, "gravity", DebugConfig::PRINT_INTERVAL, __VA_ARGS__)

// =====================================================
// GRAVITY
// =====================================================

void doGravity(
    Player& p,
    float dt
) {
    // Safety clamp dt so gravity never explodes
    float safeDt = std::min(dt, 0.033f);

    float beforeVelZ = p.vel.z;

    p.vel.z = movementApplyGravityZ(p.vel.z, PHYS.gravity, MAX_FALL_SPEED, safeDt);

    if (p.vel.z <= -MAX_FALL_SPEED && beforeVelZ + PHYS.gravity * safeDt < -MAX_FALL_SPEED) {
        GRAV_LOG(
            "[GRAVITY] Clamp fall speed -> %.3f\n",
            p.vel.z
        );
    }

    // Debug output (only when meaningful)
    if (beforeVelZ != p.vel.z) {
        GRAV_LOG(
            "[GRAVITY] vel.z %.3f -> %.3f | g=%.2f dt=%.4f\n",
            beforeVelZ,
            p.vel.z,
            PHYS.gravity,
            safeDt
        );
    }
}
