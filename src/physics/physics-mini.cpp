// Physics orchestrator.
// Calls movement subsystems in order (gravity → freeze → walk → collision → dash → jump → friction).
// No math, no logic — just delegation.
//
// Update order (each function owns its axis):
//   1. doGravity      — adds gravity to vel.z, clamps to MAX_FALL_SPEED
//   2. doFreeze       — scales velocity toward zero when held
//   3. doGroundReturn — additive slam toward ground
//   4. doWalk         — instant ground snap / CS air accelerate (horizontal only)
//   5. doCollisions   — swept capsule collision, 6 substeps
//   6. doDash/doAirDash — additive horizontal impulse
//   7. doDownDash     — additive downward impulse
//   8. doJump         — sets vel.z to jumpStrength
//   9. doFriction     — exponential decay of external impulse only
//
// Future: slide, wall-run, climb, grapple all insert new do* calls here.

#include <algorithm>
#include <cmath>
#include "input/input-commands.h"

#include "entities/player.h"
#include "world/world.h"
#include "physics/config.h"

#include "physics/physics-mini.h"
#include "physics/movement/physics-gravity.h"
#include "physics/movement/physics-walk.h"
#include "physics/movement/physics-jump.h"
#include "physics/movement/physics-dash.h"
#include "physics/movement/physics-ground-return.h"
#include "physics/movement/physics-down-dash.h"
#include "physics/movement/physics-collision.h"
#include "physics/movement/physics-friction.h"
#include "physics/movement/physics-freeze.h"

#include "physics/physics-debug-movement.h"
#include "input/input-state.h"
#include "config.h"
#include "debug/debug-log.h"

static void updateVisualFacingFromCamera(Player& p, const glm::vec3& camForward, float dt)
{
    glm::vec2 flat(camForward.x, camForward.y);
    if (glm::length(flat) < 0.0001f)
        return;

    flat = glm::normalize(flat);
    float targetYaw = glm::degrees(std::atan2(flat.y, flat.x));
    // No smoothing: player yaw matches camera yaw exactly.
    // Remote players use interpolation separately if needed.
    p.yaw = targetYaw;
}

// --------------------------------------------------
// INTERNAL physics function
// --------------------------------------------------

static void physicsMainUpdate_Internal(
    Player& p,
    const World& world,
    const glm::vec2& wishMoveXY,
    bool jumpHeld,
    bool jumpPressed,
    bool dashPressed,
    bool movementPressed,
    bool groundReturnPressed,
    bool downDashPressed,
    const glm::vec3& camForward,
    float dt,
    bool debugEnabled,
    GLFWwindow* debugWindow,
    const Camera* debugCamera,
    bool freezeHeld
){
    dt = std::min(dt, 0.033f);
    p.inputWishMove = wishMoveXY;

    const bool wasOnGround = p.wasOnGround;

    // reset per-frame flags
    p.didGroundJump = false;
    p.didAirJump    = false;
    p.didDash       = false;
    p.didLand       = false;

    doGravity(p, dt);

    doFreeze(p, freezeHeld, dt);

    doGroundReturn(p, groundReturnPressed, dt);

    doWalk(p, wishMoveXY, jumpHeld, dt);

    int steps = 6;
    float subdt = dt / steps;

    bool groundedThisFrame = false;
    p.hasWorldContact = false;

    for (int i = 0; i < steps; i++)
    {
        doCollisions(p, world, groundedThisFrame, subdt);
    }

    // Track ticks with movement held while airborne for dash quality.
    // Resets on ground contact or when movement key is released.
    if (!groundedThisFrame && movementPressed) {
        if (p.dashMovementTicks < 99) p.dashMovementTicks++;
    } else {
        p.dashMovementTicks = 0;
    }

    if (groundedThisFrame)
        doDash(p, wishMoveXY, dashPressed, camForward, dt);
    else
        doAirDash(p, wishMoveXY, dashPressed, movementPressed, true, p.dashMovementTicks, dt, camForward);

    doDownDash(p, downDashPressed, dt);

    doJump(p, jumpHeld, dt);
    if (DebugConfig::DEBUG_INPUT) {
        if (p.didGroundJump)
            Debug::log(Debug::Category::General, "[JUMP] start ground velocityZ=%.2f\n", p.vel.z);
        else if (p.didAirJump)
            Debug::log(Debug::Category::General, "[JUMP] start air velocityZ=%.2f remaining=%d\n",
                       p.vel.z, p.airJumpsLeft);
        else if (jumpHeld)
            Debug::log(Debug::Category::General,
                       "[JUMP] fail onGround=%d coyote=%.3f airJumps=%d locked=%d armed=%d\n",
                       (int)p.onGround, p.coyoteTimer, p.airJumpsLeft,
                       (int)p.airJumpLocked, (int)p.airJumpArmed);
    }

    // Reset dash when touching ground.
    // Future: centralize resets (dash, airjump, freeze, etc.) here
    //   in a single doTouchResets() call.
    if (groundedThisFrame)
    {
        p.dashAvailable = true;
    }

    // --------------------------------------------------
    // stable ground hysteresis
    // --------------------------------------------------

    // raw ground state from collision system
    p.onGround = groundedThisFrame;

    // how long since raw contact was lost
    if (groundedThisFrame)
    {
        p.groundLostTimer = 0.0f;
    }
    else
    {
        p.groundLostTimer += dt;
    }

    // remain grounded briefly after losing contact
    // this absorbs:
    // - seams
    // - tiny gaps
    // - neighboring cubes
    // - triangle switching
    // - collision jitter
    p.stableOnGround =
        groundedThisFrame ||
        (p.groundLostTimer < 0.08f);

    doFriction(p, p.stableOnGround, dt);

    // save previous airborne time BEFORE reset
    float previousAirborneTime = p.airborneTimer;

    // track stable airborne duration
    if (p.stableOnGround)
    {
        p.airborneTimer = 0.0f;
    }
    else
    {
        p.airborneTimer += dt;
    }

    // landing event only after real airtime
    if (
        !wasOnGround &&
        p.stableOnGround &&
        previousAirborneTime > 0.08f
    ){
        p.didLand = true;
    }

    // store stable state for next frame
    p.wasOnGround = p.stableOnGround;

    if (DebugConfig::DEBUG_MOVEMENT_TRACE)
    {
        Debug::logThrottled(Debug::Category::Physics, "movement-trace",
            DebugConfig::PRINT_INTERVAL,
            "[MOVEMENT TRACE] pos=(%.2f %.2f %.2f) vel=(%.2f %.2f %.2f) external=(%.2f %.2f %.2f) wish=(%.2f %.2f) grounded=%d stable=%d dash=%d freeze=%d didDash=%d didJump=%d\n",
            p.pos.x, p.pos.y, p.pos.z,
            p.vel.x, p.vel.y, p.vel.z,
            p.externalImpulse.x, p.externalImpulse.y, p.externalImpulse.z,
            wishMoveXY.x, wishMoveXY.y,
            (int)groundedThisFrame, (int)p.stableOnGround,
            (int)p.dashAvailable, (int)p.freezeActive,
            (int)p.didDash, (int)(p.didGroundJump || p.didAirJump));
    }

    updateVisualFacingFromCamera(p, camForward, dt);

    p.updateProceduralAnimation(dt, camForward, debugCamera ? debugCamera->pos : p.pos);

    // debug override
    if (debugEnabled && debugWindow && debugCamera) {
        applyDebugMovement(
            p,
            debugWindow,
            *debugCamera,
            dt
        );
    }
}


// --------------------------------------------------
// PUBLIC wrapper (called by main)
// --------------------------------------------------

void physicsMainUpdate(
    Player& p,
    const World& world,
    const InputState& input,
    float dt
){
    // Use buffered actions so quick presses survive frame drops.
    // consumeBuffered* returns true if a press happened within the buffer window,
    // and clears the buffer to prevent double-consumption.
    auto& cmd = InputCommandSystem::instance();
    bool bufferedJump = cmd.consumeBufferedJump();
    bool bufferedDash = cmd.consumeBufferedDash();
    bool bufferedDownDash = cmd.consumeBufferedDownDash();

    physicsMainUpdate_Internal(
        p,
        world,
        input.wishMoveXY,
        input.jumpHeld,
        input.jumpPressed || bufferedJump,
        input.dashPressed || bufferedDash,
        input.movementPressed,
        input.groundReturnPressed,
        input.downDashPressed || bufferedDownDash,
        input.camForward,
        dt,
        false,
        nullptr,
        nullptr,
        input.freezeHeld
    );
}
