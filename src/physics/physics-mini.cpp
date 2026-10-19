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
#include <chrono>
#include <cmath>
#include "input/input-commands.h"
#include "perf/perf.h"

#include "entities/player.h"
#include "entities/aimbody-config.h"
#include "world/world.h"
#include "physics/config.h"
#include "debug/structured-log.h"

#include "physics/physics-mini.h"
#include "physics/movement/physics-gravity.h"
#include "physics/movement/physics-walk.h"
#include "physics/movement/physics-jump.h"
#include "physics/movement/physics-dash.h"
#include "physics/movement/physics-ground-return.h"
#include "physics/movement/physics-down-dash.h"
#include "physics/movement/physics-collision.h"
#include "physics/movement/roll-movement.h"
#include "config/collision-config.h"
#include "physics/movement/physics-friction.h"
#include "physics/movement/physics-freeze.h"
#include "physics/movement/movement-conversion.h"
#include "physics/movement/movement-step.h"

#include "physics/physics-debug-movement.h"
#include "input/input-state.h"
#include "config.h"
#include "debug/debug-log.h"
#include "terminal/terminal-state.h"
#include "fister/fister-port.h"

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
    const AimBodyConfig& aimBody = AimBodyConfig::instance();
    p.yaw = aimBody.smoothAngle(p.yaw, targetYaw, dt);
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
    bool crouchHeld,
    float inputMovementHeldDuration)
{
    MovementCommand command;
    command.lifecycle = MovementLifecycleIdentity{p.spawnGeneration, 0};
    command.moveAxes = movementClampUnitOrZero(wishMoveXY);
    command.horizontalCameraForward = camForward;
    // Current camera yaw, not the stale player facing: the airborne strafe
    // eligibility layer measures per-tick camera turn from this value.
    command.lookYaw = glm::degrees(std::atan2(camForward.y, camForward.x));
    command.jumpHeld = jumpHeld;
    command.jumpPressed = jumpPressed;
    command.dashPressed = dashPressed;
    command.groundReturnPressed = groundReturnPressed;
    command.downDashPressed = downDashPressed;
    command.freezeHeld = freezeHeld;
    command.freezePressed = freezePressed || (freezeHeld && !p.freeze.freezeHeldPrev);
    command.crouchHeld = crouchHeld;
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
    bool crouchHeld,
    int subSteps,
    float inputMovementHeldDuration = 0.0f,
    const MovementConfig* overrideConfig = nullptr
){
    Perf::ScopedTimer _t("Physics");
    const MovementConfig movementConfig = overrideConfig
        ? applyRuntimeMovementTuning(*overrideConfig)
        : makeCurrentRuntimeMovementConfig();
    dt = movementClampStepDelta(dt, movementConfig);
    if (dt <= 0.0f)
        return;

    // Keep local flight responsive like freecam. The server repeats this
    // movement from the networked buttons and remains authoritative.
    // Dedicated/headless server simulation has no client multiplayer context.
    // Never dereference the optional global through MP_CONTEXT on that path.
    if (gpMpContext && gpMpContext->active && gpMpContext->flyEnabled)
    {
        // pollInput() has already converted WASD into camera-relative world
        // X/Y movement. Do not rotate it by camForward a second time.
        glm::vec3 direction(wishMoveXY.x, wishMoveXY.y, 0.0f);
        const auto& inputCommands = InputCommandSystem::instance();
        if (inputCommands.getState("freeze").held) direction.z += 1.0f;
        if (inputCommands.getState("down_dash").held) direction.z -= 1.0f;
        if (glm::length(direction) > 1.0f)
            direction = glm::normalize(direction);
        float speed = 5.0f * std::clamp(MP_CONTEXT.flySpeedMultiplier, 0.01f, 100.0f);
        if (inputCommands.getState("dash").held) speed *= 3.0f;
        p.vel = direction * speed;
        p.pos += p.vel * dt;
        p.ground.onGround = false;
        p.ground.stableOnGround = false;
        p.ground.hasWorldContact = false;
        p.ground.realWorldContactThisFrame = false;
        ++p.movementSimulationTick;
        p.updateModelWorldTransforms();
        return;
    }

    // ── Roll mode: a physical body driven by a force in the look direction ──
    // WASD becomes a force along the full camera basis; gravity and the shared
    // swept collision integrate and bounce the actor. All force/gravity/speed
    // math lives in the shared roll kernel. See
    // docs/specs/griever-monster/griever-monster.md (Slice 1).
    if (p.rig.movement.mode == "roll")
    {
        const AvatarMovementCap& m = p.rig.movement;

        // Full camera basis including pitch (spec 5.3): W/S follow the camera
        // forward, so looking down + W applies downward force; A/D use the
        // horizontal right. wishMoveXY is already camera-relative in world X/Y,
        // so recover the raw forward/side axes from it without re-reading input.
        glm::vec3 fwdFlat(camForward.x, camForward.y, 0.0f);
        if (glm::length(fwdFlat) > 1e-4f)
            fwdFlat = glm::normalize(fwdFlat);
        else
            fwdFlat = glm::vec3(1.0f, 0.0f, 0.0f);
        const glm::vec3 right = glm::normalize(
            glm::cross(fwdFlat, glm::vec3(0.0f, 0.0f, 1.0f)));
        const float moveForward =
            glm::dot(wishMoveXY, glm::vec2(fwdFlat.x, fwdFlat.y));
        const float moveSide =
            glm::dot(wishMoveXY, glm::vec2(right.x, right.y));

        glm::vec3 camFull = camForward;
        if (glm::length(camFull) > 1e-4f)
            camFull = glm::normalize(camFull);

        RollIntent intent;
        intent.direction = camFull * moveForward + right * moveSide;
        if (glm::length(intent.direction) > 1.0f)
            intent.direction = glm::normalize(intent.direction);

        p.vel = rollIntegrateVelocity(p.vel, intent, m, PHYS.gravity, dt);

        p.externalImpulse = glm::vec3(0.0f);
        p.movementContacts.clear();
        ++p.movementSimulationTick;

        bool groundedThisFrame = false;
        const int rollSteps = std::max(1, subSteps);
        const float rollSubdt = dt / (float)rollSteps;
        for (int i = 0; i < rollSteps; i++)
            doCollisions(p, world, groundedThisFrame, rollSubdt);
        p.ground.onGround = groundedThisFrame;
        p.ground.stableOnGround = groundedThisFrame;

        rollCleanupContactVelocity(
            p.vel, groundedThisFrame || !p.movementContacts.empty());

        // Presentation: roll the visible root from horizontal travel. This
        // never feeds back into position or velocity.
        const glm::quat dq = rollRotationDelta(
            glm::vec3(p.vel.x, p.vel.y, 0.0f), m.radius, dt);
        if (dq != glm::quat(1.0f, 0.0f, 0.0f, 0.0f))
        {
            p.modelRootRotation = glm::normalize(dq * p.modelRootRotation);
            p.modelRootRotationActive = true;
        }

        p.updateModelWorldTransforms();

        static uint32_t rollLogCounter = 0;
        if ((rollLogCounter++ % 30u) == 0u)
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Avatar, StructuredLevel::Important,
                "griever.roll.step", p.avatarName(), "fixed-tick roll step",
                (uint32_t)p.movementSimulationTick,
                {{"mode", m.mode},
                 {"intent", {intent.direction.x, intent.direction.y, intent.direction.z}},
                 {"vel", {p.vel.x, p.vel.y, p.vel.z}},
                 {"pos", {p.pos.x, p.pos.y, p.pos.z}},
                 {"force", m.force}, {"mass", m.mass},
                 {"gravity_scale", m.gravityScale},
                 {"grounded", groundedThisFrame},
                 {"contacts", (int)p.movementContacts.size()}},
                __FILE__, __LINE__, __FUNCTION__);

        updateVisualFacingFromCamera(p, camForward, dt);
        p.updateProceduralAnimation(dt, camForward, p.pos, true);
        return;
    }

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
        crouchHeld,
        inputMovementHeldDuration);
    command.clientSimulationTick = p.movementSimulationTick;

    MovementState movementState =
        movementStateFromPlayer(p, command.lifecycle);
    applyPreCollisionBasicMovement(movementState, command, movementConfig, dt);

    MovementStepEvents preCollisionEvents;
    applySpecialMovementPreCollision(
        movementState, command, movementConfig, dt, preCollisionEvents);
    applyMovementStateToPlayer(movementState, p);

    // Anti-tunneling: at extreme speed a single fixed sub-step can move the
    // actor far enough that the swept narrowphase's sampling/contact budget is
    // exhausted. Derive extra collision sub-steps from the actual per-tick
    // travel so each sub-step stays under maxSubStepDistance. A normal actor
    // takes its one swept fixed-tick solve; only real travel adds work.
    int steps = std::max(1, subSteps);
    {
        const CollisionConfig& collisionCfg = CollisionConfig::instance();
        const float maxSubStep = collisionCfg.maxSubStepDistance();
        const float moveSpeed = glm::length(
            movementState.baseVelocity + movementState.externalImpulse);
        const float totalTravel = moveSpeed * dt;
        if (maxSubStep > 0.0f && totalTravel > maxSubStep)
        {
            const int needed = (int)std::ceil(totalTravel / maxSubStep);
            steps = std::clamp(std::max(steps, needed), 1,
                               collisionCfg.maxSubSteps());
        }
    }
    const float subdt = dt / (float)steps;

    bool groundedThisFrame = false;

    // World contact hysteresis: persist contact for a few frames after last actual contact.
    p.ground.worldContactLostTimer = std::max(0.0f, p.ground.worldContactLostTimer - subdt);
    p.ground.hasWorldContact = p.ground.worldContactLostTimer > 0.0f;
    p.ground.realWorldContactThisFrame = false;

    const CollisionVelocityOverride velocityOverride =
        applyEffectiveVelocityForCollision(p, movementState, movementConfig);
    const float preCollisionDownwardSpeed = std::max(0.0f, -movementState.baseVelocity.z);
    const glm::vec3 positionBeforeCollision = p.pos;
    const glm::vec3 velocityBeforeCollision = p.vel;

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
    collisionFeedback.impactSpeed = preCollisionDownwardSpeed;

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

    // Sample raw collision-ground changes while a Melee fighter is crouching.
    // Keep the journal bounded while retaining the number of transitions
    // suppressed between samples and the contact data that produced them.
    if (p.usesMeleeAuthoredPose() && p.crouching &&
        groundedThisFrame != prevOnGround)
    {
        static uint64_t lastGroundTransitionLogMs = 0;
        static uint32_t suppressedGroundTransitions = 0;
        const uint64_t now = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
        if (lastGroundTransitionLogMs == 0 ||
            now - lastGroundTransitionLogMs >= 250)
        {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Collision, StructuredLevel::Important,
                "movement.ground_transition", p.username,
                "raw collision grounding changed while crouching",
                (uint32_t)p.movementSimulationTick,
                {{"previous_on_ground", prevOnGround},
                 {"collision_on_ground", groundedThisFrame},
                 {"resulting_on_ground", p.ground.onGround},
                 {"stable_on_ground", p.ground.stableOnGround},
                 {"crouching", p.crouching},
                 {"world_contact", p.ground.hasWorldContact},
                 {"real_world_contact", p.ground.realWorldContactThisFrame},
                 {"contact_count", p.movementContacts.size()},
                 {"suppressed_transitions", suppressedGroundTransitions},
                 {"ground_lost_seconds", p.ground.groundLostTimer},
                 {"position_before_collision", {positionBeforeCollision.x,
                                                  positionBeforeCollision.y,
                                                  positionBeforeCollision.z}},
                 {"position_after", {p.pos.x, p.pos.y, p.pos.z}},
                 {"velocity_before_collision", {velocityBeforeCollision.x,
                                                  velocityBeforeCollision.y,
                                                  velocityBeforeCollision.z}},
                 {"velocity_after", {p.vel.x, p.vel.y, p.vel.z}}},
                __FILE__, __LINE__, __FUNCTION__);
            lastGroundTransitionLogMs = now;
            suppressedGroundTransitions = 0;
        }
        else
        {
            ++suppressedGroundTransitions;
        }
    }

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
    int subSteps,
    const MovementConfig* overrideConfig
){
    if (FisterPort::tick(p, world, input, dt))
        return;

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
        input.crouchHeld,
        subSteps,
        input.movementHeldDuration,
        overrideConfig
    );
}
