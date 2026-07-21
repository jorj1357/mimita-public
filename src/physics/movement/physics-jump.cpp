// 07 21 2026, 10 54
/* purpose
* Adapts current Player jump behavior to shared Stage 2A jump speed helpers.
* Preserves local jump buffering, coyote timing, air-jump arming, and event flags.
* Keeps Player-specific state mutation and debug logging in the legacy wrapper.
* Does NOT perform collision, universal contact resets, dash, down dash, or freeze.
* Does NOT render, play audio, send packets, poll input, or decide authority.
* Does NOT replace the complete movement orchestrator.
*/

#include <cstdio>
#include <algorithm>

#include "physics/config.h"
#include "config/size-scaling-config.h"
#include "physics/movement/movement-step.h"
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
    const float beforeVel = p.vel.z;
    const auto& sc = SizeScalingConfig::instance().data();

    MovementConfig config;
    config.maximumDeltaSeconds = 0.033f;
    config.jumpVerticalSpeed = PHYS.jumpStrength;
    config.jumpHeightSizeExponent = sc.jumpHeightExponent;
    config.jumpBufferSeconds = JUMP_BUFFER_TIME;
    config.coyoteSeconds = COYOTE_JUMP_TIME;
    config.maximumAirJumps = AIR_JUMPS_MAX;

    MovementCommand command;
    command.jumpHeld = jumpHeld;
    command.jumpPressed = jumpPressed;

    MovementState state;
    state.baseVelocity = p.vel;
    state.sizeScale = p.sizeScale;
    state.ground.onGround = p.ground.onGround;
    state.jump.airJumpsLeft = p.jump.airJumpsLeft;
    state.jump.jumpHeldPreviously = p.jump.jumpHeldPrev;
    state.jump.airJumpLocked = p.jump.airJumpLocked;
    state.jump.airJumpArmed = p.jump.airJumpArmed;
    state.jump.jumpIntentTimerSeconds = p.jump.jumpIntentTimer;
    state.jump.coyoteTimerSeconds = p.jump.coyoteTimer;
    state.jump.didGroundJump = p.jump.didGroundJump;
    state.jump.didAirJump = p.jump.didAirJump;
    state.dash.dashAvailable = p.dash.dashAvailable;

    MovementStepEvents events;
    applyBasicJump(state, command, config, dt, &events);

    p.vel.z = state.baseVelocity.z;
    p.ground.onGround = state.ground.onGround;
    p.jump.airJumpsLeft = state.jump.airJumpsLeft;
    p.jump.jumpHeldPrev = state.jump.jumpHeldPreviously;
    p.jump.airJumpLocked = state.jump.airJumpLocked;
    p.jump.airJumpArmed = state.jump.airJumpArmed;
    p.jump.jumpIntentTimer = state.jump.jumpIntentTimerSeconds;
    p.jump.coyoteTimer = state.jump.coyoteTimerSeconds;
    p.jump.didGroundJump = state.jump.didGroundJump;
    p.jump.didAirJump = state.jump.didAirJump;
    p.dash.dashAvailable = state.dash.dashAvailable;

    if (events.didGroundJump) {
        JUMP_LOG(
            "[JUMP] GROUND vel.z %.3f -> %.3f | airJumps=%d\n",
            beforeVel,
            p.vel.z,
            p.jump.airJumpsLeft
        );
        return;
    }

    if (events.didAirJump) {
        JUMP_LOG(
            "[JUMP] AIR vel.z %.3f -> %.3f | airJumpsLeft=%d\n",
            beforeVel,
            p.vel.z,
            p.jump.airJumpsLeft
        );
        return;
    }

    if (p.jump.jumpIntentTimer > 0.0f) {
        JUMP_LOG(
            "[JUMP] BLOCKED | onGround=%d coyote=%.3f airJumps=%d didGroundJump=%d\n",
            p.ground.onGround,
            p.jump.coyoteTimer,
            p.jump.airJumpsLeft,
            (int)p.jump.didGroundJump
        );
    }
}
