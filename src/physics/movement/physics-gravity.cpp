// C:\important\quiet\n\mimita-priv-v7\src\physics\movement\physics-gravity.cpp
// feb 10 2026
/**
 * purpose
 * handles all gravity
 * should expose like
 * applygravity(args)
 * and just gets called by other files
 * maibe called bi the specific files? or the phsics main file itself, idk
 */

// Purpose:
// - Apply gravity to player vertical velocity
// - Clamp fall speed
// - Nothing else
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

    // Apply gravity
    p.vel.z += PHYS.gravity * safeDt;

    // Clamp fall speed
    if (p.vel.z < -MAX_FALL_SPEED) {
        p.vel.z = -MAX_FALL_SPEED;
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
