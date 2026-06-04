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
#include "physics/movement/physics-collision.h"
#include "physics/movement/physics-friction.h"
#include "physics/movement/physics-freeze.h"

#include "physics/physics-debug-movement.h"
#include "input/input-state.h"

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
    bool dashPressed,
    bool groundReturnPressed,
    const glm::vec3& camForward,
    float dt,
    bool debugEnabled,
    GLFWwindow* debugWindow,
    const Camera* debugCamera,
    bool freezeHeld
){
    dt = std::min(dt, 0.033f);

    // const bool wasOnGround = p.wasOnGround;
    const bool oldGrounded = p.onGround;

    // reset per-frame flags
    p.didGroundJump = false;
    p.didAirJump    = false;
    p.didDash       = false;
    p.didLand       = false;

    doGravity(p, dt);

    // now we have friction
    doFriction(p, p.onGround, dt);

    // freeze is after gravit and friction, but before everthing else
    doFreeze(p, freezeHeld, dt);

    doGroundReturn(p, groundReturnPressed, dt);
    doDash(p, wishMoveXY, dashPressed, camForward, dt);
    doWalk(p, wishMoveXY, dt);

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

    p.onGround = groundedThisFrame;

    // jump AFTER grounded so we actually know it work
    doJump(p, jumpHeld, dt);

    // this resets ur dash so u can do it again after ur grounded?
    // mar 8 2026 we reset dash in like phsics mini, or dash file, or etc
    // need to centralize this in 1 file
    // like physics-move-resets.cpp
    // that just exposes doReset(args)
    // e.g. doReset(freeze function)
    // or doReset(airjump function)
    if (groundedThisFrame)
    {
        p.dashAvailable = true;
    }

    if (p.onGround)
    {
        p.airFrames = 0;
    }
    else
    {
        p.airFrames++;
    }

    bool stableGrounded = (p.airFrames < 3);

    if (!p.wasOnGround && stableGrounded)
    {
        p.didLand = true;
    }

    p.wasOnGround = stableGrounded;

    updateVisualFacingFromCamera(p, camForward, dt);

    p.updateProceduralAnimation(dt);

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
        input.dashPressed,
        input.groundReturnPressed,
        input.camForward,
        dt,
        false,
        nullptr,
        nullptr,
        input.freezeHeld
    );
}
