// C:\important\quiet\n\mimita-priv-v7\src\physics\physics-mini.cpp
// feb 10 2026
/**
 * purpose
 * is the real phsics file, physics-mini is the real one
 * a physics file that ONLY calls functions from other files
 * theres no logic, theres no glm vec 3 vec 2 whatever 
 * its just like jump(args), dash(args), walk(args) etc
 */

// #pragma message("COMPILING physics-mini.cpp")

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
#include "analytics/analytics-manager.h"
#include "physics/physics-debug-movement.h"
#include "input/input-state.h"
#include "config.h"
#include "debug/debug-log.h"

static float shortestAngleDegrees(float from, float to)
{
    float delta = std::fmod(to - from + 540.0f, 360.0f) - 180.0f;
    return delta;
}

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

    // reset per-frame flags
    p.didGroundJump = false;
    p.didAirJump    = false;
    p.didDash       = false;
    p.didLand       = false;

    doGravity(p, dt);

    // freeze is after gravit and friction, but before everthing else
    doFreeze(p, freezeHeld, dt);

    doGroundReturn(p, groundReturnPressed, dt);
    // doDash(p, wishMoveXY, dashPressed, camForward, dt);
    // CHANGED: No dashVel — walk always runs when there's movement input, jun 6 2026
    if (p.didDash && DebugConfig::DEBUG_INPUT)
        Debug::log(Debug::Category::General, "[DASH] start direction=(%.2f %.2f) vel=(%.2f %.2f)\n",
                   wishMoveXY.x, wishMoveXY.y, p.vel.x, p.vel.y);
    if (movementPressed)
        doWalk(p, wishMoveXY, dt);

    int steps = 6;
    float subdt = dt / steps;

    bool groundedThisFrame = false;

    // World contact hysteresis: persist contact for a few frames after last actual contact.
    p.worldContactLostTimer = std::max(0.0f, p.worldContactLostTimer - subdt);
    p.hasWorldContact = p.worldContactLostTimer > 0.0f;
    p.realWorldContactThisFrame = false;

    for (int i = 0; i < steps; i++)
    {
        doCollisions(p, world, groundedThisFrame, subdt);
    }

    // --------------------------------------------------
    // Immediately sync ground state from collision (source of truth)
    // BEFORE jump, friction, landing events.
    // doJump may modify p.onGround (set false on jump).
    // --------------------------------------------------

    // Save previous state for transition detection
    bool prevOnGround = p.onGround;
    bool prevStableOnGround = p.stableOnGround;

    p.onGround = groundedThisFrame;

    if (groundedThisFrame)
        p.groundLostTimer = 0.0f;
    else
        p.groundLostTimer += dt;

    p.stableOnGround = groundedThisFrame || (p.groundLostTimer < 0.08f);

    // Track raw/was for next frame's reference
    p.rawWasOnGround = groundedThisFrame;

    // Floor-fall diagnostics
    if (DebugConfig::DEBUG_PHYSICS && !groundedThisFrame && p.vel.z < -5.0f)
    {
        Capsule debugCap = p.getCapsule();
        float feetZ = debugCap.a.z - debugCap.r;
        Debug::log(Debug::Category::Collision,
            "[DIAG] FALLING feetZ=%.3f vel=%.2f grounded=%d pos=(%.2f %.2f %.2f)\n",
            feetZ, p.vel.z, (int)groundedThisFrame, p.pos.x, p.pos.y, p.pos.z);
    }

    // Track ticks with movement held while airborne for dash quality.
    if (!groundedThisFrame && movementPressed) {
        if (p.dashMovementTicks < 99) p.dashMovementTicks++;
    } else {
        p.dashMovementTicks = 0;
    }

    // air dash
    doAirDash(p, wishMoveXY, dashPressed, movementPressed, !groundedThisFrame, p.dashMovementTicks, dt, camForward);
    if (p.didDash)
        AnalyticsManager::instance().trackMovement("dash");

    // down dash
    doDownDash(p, downDashPressed, dt);

    // jump — now reads fresh p.onGround/p.stableOnGround
    doJump(p, jumpHeld, jumpPressed, dt);

    if (p.didGroundJump)
        AnalyticsManager::instance().trackMovement(
            (p.hasWorldContact && !groundedThisFrame) ? "wall_jump" : "jump");
    else if (p.didAirJump)
        AnalyticsManager::instance().trackMovement("air_jump");
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

    // reset ALL abilities when grounded (not just dash)
    if (groundedThisFrame)
    {
        p.airJumpsLeft = AIR_JUMPS_MAX;
        p.dashAvailable = true;
        p.groundReturnAvailable = true;
        p.downDashAvailable = true;
        p.freezeAvailable = true;
    }

    doFriction(p, p.stableOnGround, dt);

    // save previous airborne time BEFORE reset
    float previousAirborneTime = p.airborneTimer;

    // track stable airborne duration
    if (p.stableOnGround)
        p.airborneTimer = 0.0f;
    else
        p.airborneTimer += dt;

    // Debug: log grounded/contact state transitions (rate-limited)
    if (DebugConfig::DEBUG_PHYSICS) {
        static float groundedDebugTimer = 0.0f;
        groundedDebugTimer += dt;
        if (p.onGround != p.rawWasOnGround || groundedDebugTimer >= 1.0f)
        {
            Debug::logThrottled(Debug::Category::Physics, "grounded", 0.5f,
                "[GROUND] raw=%d stable=%d lostTimer=%.4f airborneTimer=%.4f worldContact=%.4f didLand=%d jumpConsumed=%d landingCD=%.3f airJmp=%d locked=%d armed=%d\n",
                (int)groundedThisFrame, (int)p.stableOnGround, p.groundLostTimer,
                previousAirborneTime, p.worldContactLostTimer, (int)p.didLand,
                (int)p.jumpConsumed, p.landingCooldown,
                p.airJumpsLeft, (int)p.airJumpLocked, (int)p.airJumpArmed);
            groundedDebugTimer = 0.0f;
        }
        if (p.hasWorldContact && p.worldContactLostTimer > 0.0f)
            Debug::logThrottled(Debug::Category::Physics, "worldcontact", 0.5f,
                "[CONTACT] active timer=%.4f airJumps=%d dash=%d groundReturn=%d\n",
                p.worldContactLostTimer, p.airJumpsLeft, (int)p.dashAvailable, (int)p.groundReturnAvailable);
    }

    // Landing event: fires once per real landing using stableOnGround transition.
    p.landingCooldown = std::max(0.0f, p.landingCooldown - dt);
    bool stableLanding = !prevStableOnGround && p.stableOnGround;
    if (stableLanding && previousAirborneTime > 0.08f && p.landingCooldown <= 0.0f)
    {
        p.didLand = true;
        p.landingCooldown = 0.3f;
    }

    // store stable state for next frame
    p.wasOnGround = p.stableOnGround;

    updateVisualFacingFromCamera(p, camForward, dt);

    p.updateProceduralAnimation(dt, camForward, debugCamera ? debugCamera->pos : p.pos, movementPressed);

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
