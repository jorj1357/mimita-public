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
    float turn = std::min(1.0f, dt * 18.0f);
    p.yaw += shortestAngleDegrees(p.yaw, targetYaw) * turn;
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

    // freeze is after gravit and friction, but before everthing else
    doFreeze(p, freezeHeld, dt);

    doGroundReturn(p, groundReturnPressed, dt);
    // doDash(p, wishMoveXY, dashPressed, camForward, dt);
    // CHANGED: No dashVel — walk always runs when there's movement input, jun 6 2026
    if (p.didDash && DebugConfig::DEBUG_INPUT)
        Debug::log(Debug::Category::General, "[DASH] start direction=(%.2f %.2f) vel=(%.2f %.2f)\n",
                   wishMoveXY.x, wishMoveXY.y, p.vel.x, p.vel.y);
    // instant movement override kills momentum
    if (movementPressed)
    {
        p.externalImpulse.x = 0.0f;
        p.externalImpulse.y = 0.0f;
    }

    if (movementPressed)
        doWalk(p, wishMoveXY, dt);

    // dash after walk? 6 6 2026 
    // doDash(p, wishMoveXY, dashPressed, camForward, dt);
    // dont do dash at all 6 6 2026 ? 
    // fast push should be from gun pushing me? idk 

    // testing 4 substeps so we have even more collision checsk
    int steps = 4; // small substep count
    float subdt = dt / steps;

    // only set grounded here? not sure mar 7 2026 
    bool groundedThisFrame = false;

    for (int i = 0; i < steps; i++)
    {
        // p.pos += p.vel * subdt;
        // pass subdt not dt
        // and dont do position calc in here
        // e.g. no p.pos changing here 
        // do not do p.pos += totalVel * subdt; or antthing similar here
        // we do p.pos changing in doCollisions function
        // this file jsut calls fnuctions 

        // mar 8 2026 this added dash velocity wokring here
        // mar 8 2026 clean comments bc its toomuch 
        // mar 8 2026 we dont even use this so what 
        // glm::vec3 totalVel = p.vel + glm::vec3(p.dashVel.x, p.dashVel.y, 0.0f);

        doCollisions(p, world, groundedThisFrame, subdt);
    }

    // air dash: Space+WASD while airborne (use raw grounded state from collision)
    doAirDash(p, wishMoveXY, jumpPressed, movementPressed, !groundedThisFrame, dt);

    // down dash: Q key, always works regardless of grounded state
    doDownDash(p, downDashPressed, dt);

    // jump AFTER grounded so we actually know it work
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

    // this resets ur dash so u can do it again after ur grounded?
    // mar 8 2026 we reset dash in like phsics mini, or dash file, or etc
    // need to centralize this in 1 file
    // like physics-move-resets.cpp
    // that just exposes doReset(args)
    // e.g. doReset(freeze function)
    // or doReset(airjump function)
    // reset dash when touching ground
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

    // ok now do friction after other stuff 6 6 2026 
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

    // CHANGED: No dashVel tracking, jun 6 2026

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
    physicsMainUpdate_Internal(
        p,
        world,
        input.wishMoveXY,
        input.jumpHeld,
        input.jumpPressed,
        input.dashPressed,
        input.movementPressed,
        input.groundReturnPressed,
        input.downDashPressed,
        input.camForward,
        dt,
        false,
        nullptr,
        nullptr,
        input.freezeHeld
    );
}
