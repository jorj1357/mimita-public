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
    p.jumpIntentTimer = std::max(0.0f, p.jumpIntentTimer - dt);
    p.coyoteTimer     = std::max(0.0f, p.coyoteTimer - dt);

    // update coyote time
    if (p.onGround) {
        p.coyoteTimer = COYOTE_JUMP_TIME;
    }

    // ---------------- INTENT MANAGEMENT ----------------
    // Holding Space keeps jump intent alive every frame.
    // This enables auto-bhop: landing while holding Space causes immediate re-jump.
    if (jumpHeld)
        p.jumpIntentTimer = JUMP_BUFFER_TIME;

    bool jumpPressedThisFrame = jumpPressed || (jumpHeld && !p.jumpHeldPrev);
    if (jumpPressedThisFrame)
        p.jumpIntentTimer = JUMP_BUFFER_TIME;

    // Release re-arms air jump.
    bool jumpReleased = !jumpHeld && p.jumpHeldPrev;
    if (jumpReleased)
    {
        p.airJumpArmed = true;
        p.airJumpLocked = false;
        p.jumpConsumed = false;
    }

    // Auto-bhop: if holding Space and on ground, allow immediate re-jump.
    // This is intentionally greedy — holding Space while standing on ground
    // causes a jump. This matches the desired auto-bhop behavior.
    if (jumpHeld && p.onGround)
        p.jumpConsumed = false;

    p.jumpHeldPrev = jumpHeld;

    // ---------------- CAN JUMP ----------------
    bool onActualGround = p.onGround || p.coyoteTimer > 0.0f;
    bool wantsJump = p.jumpIntentTimer > 0.0f;

    if (!wantsJump)
        return;

    // ---------------- GROUND JUMP ----------------
    if (onActualGround && !p.jumpConsumed) {
        float beforeVel = p.vel.z;

        p.dashAvailable = true;
        p.vel.z = PHYS.jumpStrength;
        p.onGround = false;
        p.coyoteTimer = 0.0f;
        p.jumpIntentTimer = 0.0f;
        p.airJumpsLeft = AIR_JUMPS_MAX;
        p.airJumpLocked = true;
        p.airJumpArmed = false;
        p.didGroundJump = true;
        p.jumpConsumed = true;

        JUMP_LOG(
            "[JUMP] GROUND vel.z %.3f -> %.3f | airJumps=%d\n",
            beforeVel,
            p.vel.z,
            p.airJumpsLeft
        );
        return;
    }

    // ---------------- WORLD CONTACT JUMP ----------------
    if (p.hasWorldContact && !p.jumpConsumed) {
        float beforeVel = p.vel.z;

        p.vel.z = PHYS.jumpStrength;
        p.jumpIntentTimer = 0.0f;
        p.airJumpsLeft = AIR_JUMPS_MAX;
        p.didGroundJump = true;
        p.jumpConsumed = true;

        JUMP_LOG(
            "[JUMP] WORLD CONTACT vel.z %.3f -> %.3f | airJumps=%d\n",
            beforeVel,
            p.vel.z,
            p.airJumpsLeft
        );
        return;
    }

    // ---------------- AIR JUMP (press only, not hold) ----------------
    if (!p.didGroundJump &&
        p.airJumpsLeft > 0 &&
        !p.airJumpLocked &&
        p.airJumpArmed &&
        jumpPressedThisFrame)
    {
        float beforeVel = p.vel.z;

        p.vel.z = PHYS.jumpStrength;
        p.airJumpsLeft--;
        p.airJumpLocked = false;
        p.jumpIntentTimer = 0.0f;
        p.didAirJump = true;

        JUMP_LOG(
            "[JUMP] AIR vel.z %.3f -> %.3f | airJumpsLeft=%d\n",
            beforeVel,
            p.vel.z,
            p.airJumpsLeft
        );
        return;
    }

    // ---------------- FAILED ----------------
    JUMP_LOG(
        "[JUMP] BLOCKED | onGround=%d coyote=%.3f airJumps=%d\n",
        p.onGround,
        p.coyoteTimer,
        p.airJumpsLeft
    );
}
