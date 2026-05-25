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
    float dt
) {
    // ---------------- TIMERS ----------------
    // dont use this? idk mar 7 2026
    // static bool jumpHeldPrev = false;
    static float jumpIntentTimer = 0.0f;
    static float coyoteTimer = 0.0f;

    // decrement timers
    jumpIntentTimer = std::max(0.0f, jumpIntentTimer - dt);
    coyoteTimer     = std::max(0.0f, coyoteTimer - dt);

    // update coyote time
    if (p.onGround) {
        coyoteTimer = COYOTE_JUMP_TIME;
    }

    // ---------------- INPUT ----------------
    // bool jumpPressed = jumpHeld && !jumpHeldPrev;
    // set it to just be jump held so we can bhop infinite
    // bool jumpPressed = jumpHeld;
    // jumpHeldPrev = jumpHeld;
    // testing this stuff so we can have hold to jump bhop,
    // dont consume air jump when ground jumping

    // mar 7 2026 commenting this all out so we can make it so bhop holding
    // hold space = jump bhop style
    // bool jumpPressed = jumpHeld && !p.jumpHeldPrev;
    // p.jumpHeldPrev = jumpHeld;

    // if (jumpPressed) {
    //     jumpIntentTimer = JUMP_BUFFER_TIME;

    //     JUMP_LOG(
    //         "[JUMP] Pressed | buffer=%.3f\n",
    //         jumpIntentTimer
    //     );
    // }

    // hold jump keeps the buffer alive
    if (jumpHeld) {
        jumpIntentTimer = JUMP_BUFFER_TIME;
    }

    // detect release
    if (!jumpHeld && p.jumpHeldPrev)
    {
        p.airJumpArmed = true;
        p.airJumpLocked = false;
    }

    p.jumpHeldPrev = jumpHeld;

    // ---------------- CAN JUMP ----------------
    bool canGroundJump = p.onGround || coyoteTimer > 0.0f;
    bool wantsJump = jumpIntentTimer > 0.0f;

    if (!wantsJump) {
        return;
    }

    // ---------------- GROUND JUMP ----------------
    if (canGroundJump) {
        float beforeVel = p.vel.z;

        p.vel.z = PHYS.jumpStrength;
        p.onGround = false;
        coyoteTimer = 0.0f;
        jumpIntentTimer = 0.0f;

        // reset air jumps but consume one (the ground jump)
        // p.airJumpsLeft = AIR_JUMPS_MAX - 1;
        // testing mar 7 2026 dont consume a jump 
        p.airJumpsLeft = AIR_JUMPS_MAX;

        // put this HERE, so that we DO NOT use air jump 
        // when intending to only ground jump 
        p.airJumpLocked = true; 
        p.airJumpArmed = false;
        
        p.didGroundJump = true;

        JUMP_LOG(
            "[JUMP] GROUND vel.z %.3f -> %.3f | airJumps=%d\n",
            beforeVel,
            p.vel.z,
            p.airJumpsLeft
        );
        return;
    }

    // ---------------- AIR JUMP ----------------
    // if (p.airJumpsLeft > 0) {
    // testing this so we can hold jump 
    // might not include but holding jump is soooo much easier on  hands and more fun 
    // and fun is awesome

    // if (!p.didGroundJump && p.airJumpsLeft > 0) 

    // testing this one mar 7 2026 to make it so WE CAN BHOP good
    // if (!p.didGroundJump && p.airJumpsLeft > 0 && !jumpHeld)

    // air jump locked sounds a lot better lets do that mar 7 2026
    if (!p.didGroundJump && 
        p.airJumpsLeft > 0 && 
        !p.airJumpLocked && 
        p.airJumpArmed && 
        jumpHeld)
    {

        float beforeVel = p.vel.z;

        p.vel.z = PHYS.jumpStrength;
        p.airJumpsLeft--;

        // idk if this goes here but this is so i hold space while 
        // air jumping to climb walls 
        p.airJumpLocked = false;
        // do NOT set this false, this makes it so we cant hold space to climb wall
        // p.airJumpArmed = false;
        
        jumpIntentTimer = 0.0f;

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
        coyoteTimer,
        p.airJumpsLeft
    );
}
