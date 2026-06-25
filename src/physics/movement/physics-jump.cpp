// C:\important\quiet\n\mimita-priv-v7\src\physics\movement\physics-jump.cpp
// feb 10 2026
/**
 * purpose
 * handle all logic for jumps
 * if plr press jump, jump
 * should expose jump(args) to other files
 * this also handles doublejump
 * so like doublejump(args) as well
 * also handles bools and stuff
 * for audio to not be spamming when we hold jump down
 * maibe that just goes in the audio file itself? u can hold jump all u want, 
 * but cant plau the audio whenevr u want?
 * idk.
 * i think its better to put a cap on hwo man times u can jump like u cant just 
 * jump forever, infinitelu,
 * put liek a 0.2s timer between jumps
 * but u can hold jump fro as long as u can
 * maibe even small, like 0.01s 
 */

// C:\important\quiet\n\mimita-priv-v7\src\physics\movement\physics-jump.cpp
// feb 10 2026
//
// Purpose:
// - Ground jump
// - Air jump (double jump)
// - Jump buffering
// - Coyote time
//
// Uses physics/config.h
// Debug heavy
// No collision / no grounding detection

#include <cstdio>
#include <algorithm>

#include "physics/config.h"
#include "entities/player.h"
#include "debug/debug-log.h"

// =====================================================
// DEBUG
// =====================================================

#define JUMP_LOG(...) Debug::logThrottled(Debug::Category::Physics, "jump", DebugConfig::PRINT_INTERVAL, __VA_ARGS__)

// =====================================================
// JUMP
// =====================================================

void doJump(
    Player& p,
    bool jumpHeld,
    bool jumpPressed,
    float dt
) {
    // decrement timers
    p.jump.jumpIntentTimer = std::max(0.0f, p.jump.jumpIntentTimer - dt);
    p.jump.coyoteTimer     = std::max(0.0f, p.jump.coyoteTimer - dt);

    // update coyote time
    if (p.ground.onGround) {
        p.jump.coyoteTimer = COYOTE_JUMP_TIME;
    }

    // ---------------- INTENT MANAGEMENT ----------------
    // Holding Space keeps jump intent alive every frame.
    // This enables continuous jump attempts: landing while holding Space causes immediate re-jump.
    if (jumpHeld)
        p.jump.jumpIntentTimer = JUMP_BUFFER_TIME;

    bool jumpPressedThisFrame = jumpPressed || (jumpHeld && !p.jump.jumpHeldPrev);
    if (jumpPressedThisFrame)
        p.jump.jumpIntentTimer = JUMP_BUFFER_TIME;

    // Release re-arms air jump for the next press.
    // Clear jumpIntentTimer so the release itself does not trigger a jump.
    bool jumpReleased = !jumpHeld && p.jump.jumpHeldPrev;
    if (jumpReleased)
    {
        p.jump.airJumpArmed = true;
        p.jump.airJumpLocked = false;
        p.jump.jumpIntentTimer = 0.0f;
    }

    p.jump.jumpHeldPrev = jumpHeld;

    // ---------------- CAN JUMP ----------------
    bool onActualGround = p.ground.onGround || p.jump.coyoteTimer > 0.0f;
    bool wantsJump = p.jump.jumpIntentTimer > 0.0f;

    if (!wantsJump)
        return;

    // ---------------- GROUND JUMP (continuous while holding Space) ----------------
    if (onActualGround) {
        float beforeVel = p.vel.z;

        p.dash.dashAvailable = true;
        p.vel.z = PHYS.jumpStrength;
        p.ground.onGround = false;
        p.jump.coyoteTimer = 0.0f;
        p.jump.jumpIntentTimer = 0.0f;
        p.jump.airJumpsLeft = AIR_JUMPS_MAX;
        p.jump.airJumpLocked = true;
        p.jump.airJumpArmed = false;
        p.jump.didGroundJump = true;

        JUMP_LOG(
            "[JUMP] GROUND vel.z %.3f -> %.3f | airJumps=%d\n",
            beforeVel,
            p.vel.z,
            p.jump.airJumpsLeft
        );
        return;
    }

    // ---------------- AIR JUMP (requires release to re-arm) ----------------
    // Resets: applyTouchResets (on any collision contact) sets airJumpsLeft = AIR_JUMPS_MAX and airJumpArmed = true.
    // Consumed: on jump, airJumpsLeft is decremented and airJumpArmed is set to false.
    // This prevents holding Space from consuming multiple air jumps.
    if (p.jump.airJumpsLeft > 0 &&
        p.jump.airJumpArmed)
    {
        float beforeVel = p.vel.z;

        p.vel.z = PHYS.jumpStrength;
        p.jump.airJumpsLeft--;
        p.jump.airJumpArmed = false;
        p.jump.airJumpLocked = true;
        p.jump.jumpIntentTimer = 0.0f;
        p.jump.didAirJump = true;

        JUMP_LOG(
            "[JUMP] AIR vel.z %.3f -> %.3f | airJumpsLeft=%d\n",
            beforeVel,
            p.vel.z,
            p.jump.airJumpsLeft
        );
        return;
    }

    // ---------------- FAILED ----------------
    JUMP_LOG(
        "[JUMP] BLOCKED | onGround=%d coyote=%.3f airJumps=%d didGroundJump=%d\n",
        p.ground.onGround,
        p.jump.coyoteTimer,
        p.jump.airJumpsLeft,
        (int)p.jump.didGroundJump
    );
}
