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
#include "perf/perf.h"

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
    bool movementJustPressed,
    bool groundReturnPressed,
    bool downDashPressed,
    const glm::vec3& camForward,
    float dt,
    bool debugEnabled,
    GLFWwindow* debugWindow,
    const Camera* debugCamera,
    bool freezeHeld,
    int subSteps,
    float inputMovementHeldDuration = 0.0f
){
    Perf::ScopedTimer _t("Physics");
    dt = std::min(dt, 0.033f);
    p.inputWishMove = wishMoveXY;

    // reset per-frame flags
    p.jump.didGroundJump = false;
    p.jump.didAirJump    = false;
    p.dash.didDash       = false;
    p.dash.didDownDash   = false;
    p.ground.didLand       = false;
    p.freeze.didFreeze     = false;

    doGravity(p, dt);

    // freeze is after gravit and friction, but before everthing else
    doFreeze(p, freezeHeld, dt);

    doGroundReturn(p, groundReturnPressed, dt);
    if (p.dash.didDash && DebugConfig::DEBUG_INPUT)
        Debug::log(Debug::Category::General, "[DASH] start direction=(%.2f %.2f) vel=(%.2f %.2f)\n",
                   wishMoveXY.x, wishMoveXY.y, p.vel.x, p.vel.y);

    int steps = subSteps;
    float subdt = dt / steps;

    bool groundedThisFrame = false;

    // World contact hysteresis: persist contact for a few frames after last actual contact.
    p.ground.worldContactLostTimer = std::max(0.0f, p.ground.worldContactLostTimer - subdt);
    p.ground.hasWorldContact = p.ground.worldContactLostTimer > 0.0f;
    p.ground.realWorldContactThisFrame = false;

    // Down dash must run BEFORE collision so the collision system
    // processes the downward velocity on the same frame — no one-frame delay.
    doDownDash(p, downDashPressed, dt);

    for (int i = 0; i < steps; i++)
    {
        doCollisions(p, world, groundedThisFrame, subdt);
    }

    // --------------------------------------------------
    // Immediately sync ground state from collision (source of truth)
    // BEFORE jump, friction, landing events.
    // doJump may modify p.ground.onGround (set false on jump).
    // --------------------------------------------------

    // Save previous state for transition detection
    bool prevOnGround = p.ground.onGround;
    bool prevStableOnGround = p.ground.stableOnGround;

    p.ground.onGround = groundedThisFrame;

    if (groundedThisFrame)
        p.ground.groundLostTimer = 0.0f;
    else
        p.ground.groundLostTimer += dt;

    p.ground.stableOnGround = groundedThisFrame || (p.ground.groundLostTimer < 0.08f);

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
        if (p.dash.dashMovementTicks < 99) p.dash.dashMovementTicks++;
    } else {
        p.dash.dashMovementTicks = 0;
    }

    // Clear external velocity when player takes direct movement control.
    // Movement keys, dash, and freeze get priority over recoil/knockback.
    // Jump does NOT trigger this clear (preserves external velocity through jumps).
    // Preserve upward external impulse Z so knockback still counteracts downward velocity.
    if (movementPressed || dashPressed || freezeHeld)
    {
        float upZ = p.externalImpulse.z > 0.0f ? p.externalImpulse.z : 0.0f;
        p.externalImpulse = glm::vec3(0.0f);
        p.externalImpulse.z = upZ;
    }

    // Walk applies movement input — ground and air (sets base horizontal velocity).
    // Skip walk when tick-perfect friction override is active — preserves momentum.
    if (movementPressed && p.dash.frictionOverride >= 1.0f) {
        doWalk(p, wishMoveXY, groundedThisFrame, dt);
    } else if (movementPressed && p.dash.frictionOverride < 1.0f) {
        Debug::logThrottled(Debug::Category::Physics, "walk-skip", 0.5f,
            "[VEL MODIFY] source=walk_suppressed tickPerfect=1 speed=%.1f\n",
            glm::length(glm::vec2(p.vel.x, p.vel.y)));
    }

    // Air dash — single impulse triggered by Left Shift while airborne.
    // The dash impulse fires ONCE. Low-friction mode (from tick perfect) is handled
    // independently by doFriction reading p.dash.frictionOverride.
    if (!groundedThisFrame && dashPressed && p.dash.dashAvailable) {
        doAirDash(p, wishMoveXY, true, movementPressed, !groundedThisFrame,
                  p.dash.dashMovementTicks, inputMovementHeldDuration, dt, camForward);
    }

    // Jump — space is always jump, never dash
    doJump(p, jumpHeld, jumpPressed, dt);

    // Friction override management
    // Do NOT reset on the same frame the dash fires — the override was just set by doAirDash.
    if (p.dash.frictionOverride < 1.0f && !p.dash.didDash) {
        // The next frame after the dash, any meaningful input restores normal friction.
        bool inputDetected = movementJustPressed || jumpHeld || dashPressed || freezeHeld || downDashPressed;
        bool abilityUsed = p.dash.didDownDash || p.freeze.didFreeze;
        if (inputDetected || abilityUsed) {
            float speedBefore = glm::length(glm::vec2(p.vel.x, p.vel.y));
            p.dash.frictionOverride = 1.0f;
            p.dash.tickPerfectDash = false;
            Debug::log(Debug::Category::Physics, "[FRICTION OVERRIDE] disabled inputDetected=%d speed=%.1f\n", (int)inputDetected, speedBefore);
        }
    }
    if (DebugConfig::DEBUG_INPUT) {
        if (p.jump.didGroundJump)
            Debug::log(Debug::Category::General, "[JUMP] start ground velocityZ=%.2f\n", p.vel.z);
        else if (p.jump.didAirJump)
            Debug::log(Debug::Category::General, "[JUMP] start air velocityZ=%.2f remaining=%d\n",
                       p.vel.z, p.jump.airJumpsLeft);
        else if (jumpHeld)
            Debug::log(Debug::Category::General,
                       "[JUMP] fail onGround=%d coyote=%.3f airJumps=%d locked=%d armed=%d\n",
                       (int)p.ground.onGround, p.jump.coyoteTimer, p.jump.airJumpsLeft,
                       (int)p.jump.airJumpLocked, (int)p.jump.airJumpArmed);
    }

    // reset ALL abilities when grounded (not just dash)
    // applyTouchResets (called during collision contact resolution, above) already
    // restores airJumpsLeft, airJumpArmed, dash, etc. This block is a safety net
    // for grounded frames where applyTouchResets may not have fired.
    // NOTE: do NOT re-arm airJumpArmed here — it runs AFTER doJump and would undo
    // the air-jump-armed=false that a ground jump just set, causing an immediate
    // free air jump while holding Space on the next frame.
    if (groundedThisFrame)
    {
        p.jump.airJumpsLeft = AIR_JUMPS_MAX;
        p.dash.dashAvailable = true;
        p.groundReturn.available = true;
        p.dash.downDashAvailable = true;
        p.freeze.freezeAvailable = true;
    }

    doFriction(p, p.ground.stableOnGround, dt);

    // save previous airborne time BEFORE reset
    float previousAirborneTime = p.ground.airborneTimer;

    // track stable airborne duration
    if (p.ground.stableOnGround)
        p.ground.airborneTimer = 0.0f;
    else
        p.ground.airborneTimer += dt;

    // Debug: log grounded/contact state transitions (rate-limited)
    if (DebugConfig::DEBUG_PHYSICS) {
        static float groundedDebugTimer = 0.0f;
        groundedDebugTimer += dt;
        if (p.ground.onGround != prevOnGround || groundedDebugTimer >= 1.0f)
        {
            Debug::logThrottled(Debug::Category::Physics, "grounded", 0.5f,
                "[GROUND] raw=%d stable=%d lostTimer=%.4f airborneTimer=%.4f worldContact=%.4f didLand=%d landingCD=%.3f airJmp=%d locked=%d armed=%d\n",
                (int)groundedThisFrame, (int)p.ground.stableOnGround, p.ground.groundLostTimer,
                previousAirborneTime, p.ground.worldContactLostTimer, (int)p.ground.didLand,
                p.ground.landingCooldown,
                p.jump.airJumpsLeft, (int)p.jump.airJumpLocked, (int)p.jump.airJumpArmed);
            groundedDebugTimer = 0.0f;
        }
        if (p.ground.hasWorldContact && p.ground.worldContactLostTimer > 0.0f)
            Debug::logThrottled(Debug::Category::Physics, "worldcontact", 0.5f,
                "[CONTACT] active timer=%.4f airJumps=%d dash=%d groundReturn=%d\n",
                p.ground.worldContactLostTimer, p.jump.airJumpsLeft, (int)p.dash.dashAvailable, (int)p.groundReturn.available);
    }

    // Landing event: fires once per real landing using stableOnGround transition.
    p.ground.landingCooldown = std::max(0.0f, p.ground.landingCooldown - dt);
    bool stableLanding = !prevStableOnGround && p.ground.stableOnGround;
    if (stableLanding && previousAirborneTime > 0.08f && p.ground.landingCooldown <= 0.0f)
    {
        p.ground.didLand = true;
        p.ground.landingCooldown = 0.3f;
    }

    // store stable state for next frame
    p.ground.wasOnGround = p.ground.stableOnGround;

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
    float dt,
    int subSteps
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
        input.movementJustPressed,
        input.groundReturnPressed,
        input.downDashPressed || bufferedDownDash,
        input.camForward,
        dt,
        false,
        nullptr,
        nullptr,
        input.freezeHeld,
        subSteps,
        input.movementHeldDuration
    );
}
