// 07 21 2026, 16 30
/* purpose
* Orchestrates the local Player/NPC physics tick around the collision boundary.
* Adapts legacy Player/InputState data to the shared movement kernel phases.
* Keeps collision, animation, and debug movement ordered around deterministic movement state.
* Does NOT own movement formulas, collision sweeps, input polling, packets, or rendering.
* Does NOT run server authority, prediction history, replay serialization, or weapon logic.
* Does NOT keep Ground Return in the active target movement path.
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
#include "physics/movement/movement-conversion.h"
#include "physics/movement/movement-step.h"

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

struct CollisionVelocityOverride {
    bool active = false;
    float horizontalPassThrough = 1.0f;
    glm::vec3 storedBaseVelocity{0.0f};
    glm::vec3 storedExternalImpulse{0.0f};
};

static MovementCommand buildMovementCommandFromPhysicsInputs(
    const Player& p,
    const glm::vec2& wishMoveXY,
    bool jumpHeld,
    bool jumpPressed,
    bool dashPressed,
    bool movementPressed,
    bool movementJustPressed,
    bool groundReturnPressed,
    bool downDashPressed,
    const glm::vec3& camForward,
    bool freezeHeld,
    bool freezePressed,
    float inputMovementHeldDuration)
{
    MovementCommand command;
    command.lifecycle = MovementLifecycleIdentity{p.spawnGeneration, 0};
    command.moveAxes = movementClampUnitOrZero(wishMoveXY);
    command.horizontalCameraForward = camForward;
    command.lookYaw = p.yaw;
    command.jumpHeld = jumpHeld;
    command.jumpPressed = jumpPressed;
    command.dashPressed = dashPressed;
    command.groundReturnPressed = groundReturnPressed;
    command.downDashPressed = downDashPressed;
    command.freezeHeld = freezeHeld;
    command.freezePressed = freezePressed || (freezeHeld && !p.freeze.freezeHeldPrev);
    command.movementHeldDurationSeconds = inputMovementHeldDuration;

    const MovementDirectionTransition transition =
        movementDirectionTransition(p.inputWishMove, command.moveAxes);
    command.movementDirectionPressed =
        movementPressed || transition.currentPressed;
    command.movementDirectionFreshPressed =
        movementJustPressed || transition.freshPress;
    command.movementDirectionReleased = transition.released;
    command.movementDirectionChanged = transition.directionChanged;
    return command;
}

static CollisionVelocityOverride applyEffectiveVelocityForCollision(
    Player& p,
    const MovementState& state,
    const MovementConfig& config)
{
    CollisionVelocityOverride overrideState;
    overrideState.horizontalPassThrough =
        freezeHorizontalPassThrough(state.freeze, config);
    overrideState.storedBaseVelocity = state.baseVelocity;
    overrideState.storedExternalImpulse = state.externalImpulse;

    if (!state.freeze.active ||
        overrideState.horizontalPassThrough >= 1.0f) {
        return overrideState;
    }

    const MovementVelocityView view =
        movementVelocityViewForCollision(state, config);
    p.vel = view.effectiveBaseVelocity;
    p.externalImpulse = view.effectiveExternalImpulse;
    overrideState.active = true;
    return overrideState;
}

static void reconcileEffectiveCollisionVelocity(
    MovementState& state,
    const CollisionVelocityOverride& overrideState,
    const MovementConfig& config)
{
    if (!overrideState.active)
        return;

    const float restoreThreshold =
        std::max(config.freezeDashMinimumPassThrough, MOVEMENT_INPUT_EPSILON);
    if (overrideState.horizontalPassThrough > restoreThreshold) {
        state.baseVelocity.x /= overrideState.horizontalPassThrough;
        state.baseVelocity.y /= overrideState.horizontalPassThrough;
        state.baseVelocity.z /= overrideState.horizontalPassThrough;
        state.externalImpulse.x /= overrideState.horizontalPassThrough;
        state.externalImpulse.y /= overrideState.horizontalPassThrough;
        state.externalImpulse.z /= overrideState.horizontalPassThrough;
    } else {
        state.baseVelocity.x = overrideState.storedBaseVelocity.x;
        state.baseVelocity.y = overrideState.storedBaseVelocity.y;
        state.baseVelocity.z = overrideState.storedBaseVelocity.z;
        state.externalImpulse.x = overrideState.storedExternalImpulse.x;
        state.externalImpulse.y = overrideState.storedExternalImpulse.y;
        state.externalImpulse.z = overrideState.storedExternalImpulse.z;
    }
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
    bool freezePressed,
    int subSteps,
    float inputMovementHeldDuration = 0.0f
){
    Perf::ScopedTimer _t("Physics");
    const MovementConfig movementConfig = makeCurrentRuntimeMovementConfig();
    dt = movementClampStepDelta(dt, movementConfig);
    if (dt <= 0.0f)
        return;

    ++p.movementSimulationTick;
    p.movementContacts.clear();

    MovementCommand command = buildMovementCommandFromPhysicsInputs(
        p,
        wishMoveXY,
        jumpHeld,
        jumpPressed,
        dashPressed,
        movementPressed,
        movementJustPressed,
        groundReturnPressed,
        downDashPressed,
        camForward,
        freezeHeld,
        freezePressed,
        inputMovementHeldDuration);
    command.clientSimulationTick = p.movementSimulationTick;

    MovementState movementState =
        movementStateFromPlayer(p, command.lifecycle);
    applyPreCollisionBasicMovement(movementState, command, movementConfig, dt);

    MovementStepEvents preCollisionEvents;
    applySpecialMovementPreCollision(
        movementState, command, movementConfig, dt, preCollisionEvents);
    applyMovementStateToPlayer(movementState, p);

    const int steps = std::max(1, subSteps);
    const float subdt = dt / (float)steps;

    bool groundedThisFrame = false;

    // World contact hysteresis: persist contact for a few frames after last actual contact.
    p.ground.worldContactLostTimer = std::max(0.0f, p.ground.worldContactLostTimer - subdt);
    p.ground.hasWorldContact = p.ground.worldContactLostTimer > 0.0f;
    p.ground.realWorldContactThisFrame = false;

    const CollisionVelocityOverride velocityOverride =
        applyEffectiveVelocityForCollision(p, movementState, movementConfig);

    for (int i = 0; i < steps; i++)
    {
        doCollisions(p, world, groundedThisFrame, subdt);
    }

    MovementState collisionState =
        movementStateFromPlayer(p, command.lifecycle);
    reconcileEffectiveCollisionVelocity(
        collisionState, velocityOverride, movementConfig);

    MovementCollisionFeedback collisionFeedback;
    collisionFeedback.onGround = groundedThisFrame;
    collisionFeedback.hasWorldContact = p.ground.hasWorldContact;
    collisionFeedback.realWorldContactThisFrame =
        p.ground.realWorldContactThisFrame;
    collisionFeedback.simulationTick = command.clientSimulationTick;
    collisionFeedback.contacts = p.movementContacts;

    const bool prevOnGround = collisionState.ground.onGround;
    const float previousAirborneTime =
        collisionState.ground.airborneTimerSeconds;

    MovementStepResult movementResult =
        applyPostCollisionMovementWithSpecials(collisionState,
                                               command,
                                               movementConfig,
                                               collisionFeedback,
                                               dt,
                                               preCollisionEvents);
    applyMovementStateToPlayer(movementResult.state, p);

    // Floor-fall diagnostics
    if (DebugConfig::DEBUG_PHYSICS && !groundedThisFrame && p.vel.z < -5.0f)
    {
        Capsule debugCap = p.getCapsule();
        float feetZ = debugCap.a.z - debugCap.r;
        Debug::log(Debug::Category::Collision,
            "[DIAG] FALLING feetZ=%.3f vel=%.2f grounded=%d pos=(%.2f %.2f %.2f)\n",
            feetZ, p.vel.z, (int)groundedThisFrame, p.pos.x, p.pos.y, p.pos.z);
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

    updateVisualFacingFromCamera(p, camForward, dt);

    p.updateProceduralAnimation(dt,
                                camForward,
                                debugCamera ? debugCamera->pos : p.pos,
                                command.movementDirectionPressed);

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
        input.freezePressed,
        subSteps,
        input.movementHeldDuration
    );
}
