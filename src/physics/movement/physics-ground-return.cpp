// Purpose:
// - Handle ground return charges + slam impulse
// - Additive velocity: does not overwrite existing momentum
// - No collision / no grounding logic
//
// Future: if ground return needs multiple charges or cooldowns,
//   add tracking here rather than a separate file.
//
// Uses physics/config.h
// Debug heavy

#include <cstdio>
#include <algorithm>

#include "physics/config.h"
#include "entities/player.h"
#include "debug/debug-log.h"

// =====================================================
// DEBUG
// =====================================================

#define GR_LOG(...) Debug::logThrottled(Debug::Category::Physics, "ground-return", DebugConfig::PRINT_INTERVAL, __VA_ARGS__)

// =====================================================
// GROUND RETURN
// =====================================================

void doGroundReturn(
    Player& p,
    bool groundReturnPressed,
    float dt
) {
    // ---------------- INPUT ----------------
    if (!groundReturnPressed)
        return;

    // only usable once until touching an object
    if (!p.groundReturnAvailable)
    {
        GR_LOG("[GROUND_RETURN] blocked (not available)\n");
        return;
    }

    // block if already grounded
    if (p.onGround)
    {
        GR_LOG("[GROUND_RETURN] blocked (already grounded)\n");
        return;
    }

    // ---------------- EXECUTE ----------------

    float beforeVelZ = p.vel.z;

    // Additive: preserves existing upward/downward momentum.
    p.vel.z += GROUND_RETURN_SPEED;

    // consume ability until next surface touch
    p.groundReturnAvailable = false;

    GR_LOG(
        "[GROUND_RETURN] EXECUTE vel.z %.3f -> %.3f\n",
        beforeVelZ,
        p.vel.z
    );
}
