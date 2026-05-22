// C:\important\quiet\n\mimita-priv-v7\src\physics\movement\physics-ground-return.cpp
// feb 10 2026
/**
 * purpose
 * handles all logic for ground return
 * just exposes like doGroundReturn(args) or something
 * gravities here are called from gravit function file
 * like applyGravity(args)
 */

// Purpose:
// - Handle ground return charges + slam impulse
// - Vertical velocity override only
// - No collision / no grounding logic
//
// Uses physics/config.h
// Debug heavy

#include <cstdio>
#include <algorithm>

#include "physics/config.h"
#include "entities/player.h"

// =====================================================
// DEBUG
// =====================================================

#define PHYS_DEBUG_GROUND_RETURN 1

#if PHYS_DEBUG_GROUND_RETURN
    #define GR_LOG(...) std::printf(__VA_ARGS__)
#else
    #define GR_LOG(...)
#endif

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

    // strong downward slam
    p.vel.z = GROUND_RETURN_SPEED;

    // consume ability until next surface touch
    p.groundReturnAvailable = false;

    GR_LOG(
        "[GROUND_RETURN] EXECUTE vel.z %.3f -> %.3f\n",
        beforeVelZ,
        p.vel.z
    );
}