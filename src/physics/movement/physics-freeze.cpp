// C:\important\quiet\n\mimita-priv-v7\src\physics\movement\physics-freeze.cpp
// mar 8 2026
/**
 * purpose
 * freeze logic
 * exposes doFreeze(args)
 * like this 
 * Hold G = 
At 0 seconds
Ur velocity is 0 
At 5 seconds
Ur velocity is back to normal gravity
Make it so seconds 0 to 2.5 is like 
0 velocity , 0.5 velocity, 1.2 velocity, super small
Then 2.5 and above is like
1.2 , 2.8, 3.9, 5.9, 10.5, increase faster as it gets to 5 sec
And 5 sec = normal falling velocity i think 
 */

#include <cstdio>
#include <algorithm>
#include <cmath>

#include "physics/config.h"
#include "entities/player.h"
#include "audio/audio.h"
#include "physics/movement/physics-freeze.h"
#include "debug/debug-log.h"

// =====================================================
// DEBUG
// =====================================================

#define FREEZE_LOG(...) Debug::logThrottled(Debug::Category::Physics, "freeze", DebugConfig::PRINT_INTERVAL, __VA_ARGS__)


// =====================================================
// FREEZE VELOCITY CURVE
// =====================================================

static float freezeVelocityMultiplier(float t)
{
    // t = 0 → 5 seconds

    if (t < 2.5f)
    {
        float x = t / 2.5f;
        return x * x * 0.2f;
    }

    float x = (t - 2.5f) / 2.5f;

    return 0.2f + x * x * 0.8f;
}


// =====================================================
// FREEZE
// =====================================================

void doFreeze(
    Player& p,
    bool freezeHeld,
    float dt
)
{
    // --------------------------------------------------
    // EDGE DETECT
    // --------------------------------------------------

    bool freezeJustPressed = freezeHeld && !p.freezeHeldPrev;
    bool freezeJustReleased = !freezeHeld && p.freezeHeldPrev;

    p.freezeHeldPrev = freezeHeld;


    // --------------------------------------------------
    // START FREEZE
    // --------------------------------------------------

    if (freezeJustPressed)
    {
        if (!p.freezeAvailable)
        {
            FREEZE_LOG("[FREEZE] blocked (not available)\n");
            return;
        }

        // set velocit to 0 here? idk
        p.vel = glm::vec3(0.0f);
        p.freezeActive = true;
        p.freezeTimer = 0.0f;
        p.freezeAvailable = false;
        p.freezeHoldSoundPlayed = false;

        playSound("entity/player/freezebegin");

        FREEZE_LOG("[FREEZE] begin\n");
    }


    // --------------------------------------------------
    // END FREEZE
    // --------------------------------------------------

    if (freezeJustReleased && p.freezeActive)
    {
        p.freezeActive = false;

        FREEZE_LOG("[FREEZE] end\n");

        playSound("entity/player/freezeend");
    }


    // --------------------------------------------------
    // HOLD FREEZE
    // --------------------------------------------------

    if (!p.freezeActive)
        return;

    p.freezeTimer += dt;

    // if (!p.freezeHoldSoundPlayed && p.freezeTimer >= 0.5f)
    // this is annouing so never plau it
    if (!p.freezeHoldSoundPlayed && p.freezeTimer >= 999.0f)
    {
        playSound("entity/player/freezehold");
        p.freezeHoldSoundPlayed = true;
    }

    if (p.freezeTimer > FREEZE_MAX_TIME)
        p.freezeTimer = FREEZE_MAX_TIME;

    float mult = freezeVelocityMultiplier(p.freezeTimer);

    // reduce velocity

    // mar 8 2026 this is v1 
    // we want to set all velocities to 0 and then scale nicely to the default velocities 
    // over 5 sec
    // but the new function we testing
    // mar 8 2026 im using this bc freeze applies to all axises not just z
    p.vel.x *= mult;
    p.vel.y *= mult;
    p.vel.z *= mult;

    // testing this might not work mar 8 2026 
    // freeze controls gravity influence instead of scaling velocity

    // float gravityScale = freezeVelocityMultiplier(p.freezeTimer);

    // // apply scaled gravity manually
    // p.vel.z += PHYS.gravity * gravityScale * dt;

    FREEZE_LOG(
        "[FREEZE] t=%.2f mult=%.3f\n",
        p.freezeTimer,
        mult
    );

    // TODO sound
    // playSoundLoop("entity/player/freezehold");
}
