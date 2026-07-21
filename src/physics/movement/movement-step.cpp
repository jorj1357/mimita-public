// 07 21 2026, 10 54
/* purpose
* Implements the shared Stage 2A basic movement kernel and pure formula helpers.
* Preserves current local ordering around collision by splitting pre and post phases.
* Reuses the same helpers legacy walking, gravity, jump speed, friction, and impulse code call.
* Does NOT perform collision sweeps, packet handling, rendering, audio, or authority decisions.
* Does NOT implement dash, down dash, freeze, Ground Return, or contact reset simulation.
* Does NOT own presentation state or mutate Player directly.
*/

#include "physics/movement/movement-step.h"

#include <algorithm>
#include <cmath>

namespace {

float positiveOrDefault(float value, float fallback)
{
    return value > 0.0f ? value : fallback;
}

float expDecay(float amount, float dt)
{
    return std::exp(-amount * dt);
}

void resetBasicOneTickState(MovementState& state)
{
    state.jump.didGroundJump = false;
    state.jump.didAirJump = false;
    state.ground.didLand = false;
    state.dash.didDash = false;
    state.downDash.didDownDash = false;
    state.freeze.didFreeze = false;
}

void applyBasicFrictionOverrideRecovery(MovementState& state,
                                        const MovementCommand& command)
{
    if (state.dash.frictionOverride >= 1.0f || state.dash.didDash)
        return;

    const bool inputDetected =
        command.movementDirectionFreshPressed ||
        command.dashPressed ||
        command.freezeHeld ||
        command.downDashPressed;
    const bool abilityUsed = state.downDash.didDownDash || state.freeze.didFreeze;
    if (!inputDetected && !abilityUsed)
        return;

    state.dash.frictionOverride = 1.0f;
    state.dash.tickPerfectDash = false;
}

void applyGroundedResourceSafetyReset(MovementState& state,
                                      const MovementConfig& config,
                                      bool groundedThisFrame)
{
    if (!groundedThisFrame)
        return;

    state.jump.airJumpsLeft = config.maximumAirJumps;
    state.dash.dashAvailable = true;
    state.groundReturn.available = true;
    state.downDash.available = true;
    state.freeze.available = true;
}

void applyBasicLandingTimers(MovementState& state,
                             const MovementConfig& config,
                             bool previousStableOnGround,
                             float previousAirborneSeconds,
                             float fixedDt,
                             MovementStepEvents& events)
{
    if (state.ground.stableOnGround)
        state.ground.airborneTimerSeconds = 0.0f;
    else
        state.ground.airborneTimerSeconds += fixedDt;

    state.ground.landingCooldownSeconds =
        std::max(0.0f, state.ground.landingCooldownSeconds - fixedDt);

    const bool stableLanding =
        !previousStableOnGround && state.ground.stableOnGround;
    if (stableLanding &&
        previousAirborneSeconds > config.landingMinimumAirborneSeconds &&
        state.ground.landingCooldownSeconds <= 0.0f) {
        state.ground.didLand = true;
        state.ground.landingCooldownSeconds = config.landingCooldownResetSeconds;
        events.didLand = true;
    }

    state.ground.wasOnGround = state.ground.stableOnGround;
}

} // namespace

float movementClampStepDelta(float dt, const MovementConfig& config)
{
    if (!std::isfinite(dt) || dt <= 0.0f)
        return 0.0f;
    return std::min(dt, positiveOrDefault(config.maximumDeltaSeconds, 0.033f));
}

float movementSafeSizeScale(float sizeScale)
{
    if (!std::isfinite(sizeScale))
        return 0.001f;
    return std::max(sizeScale, 0.001f);
}

float movementSizeScaleFactor(float sizeScale, float exponent)
{
    return std::pow(movementSafeSizeScale(sizeScale), exponent);
}

float movementScaledGroundSpeed(const MovementConfig& config, float sizeScale)
{
    return config.groundSpeed *
           movementSizeScaleFactor(sizeScale, config.movementSpeedSizeExponent);
}

float movementScaledAirSpeed(const MovementConfig& config, float sizeScale)
{
    return config.airSpeed *
           movementSizeScaleFactor(sizeScale, config.movementSpeedSizeExponent);
}

float movementScaledJumpVelocity(const MovementConfig& config, float sizeScale)
{
    return movementScaledJumpVelocity(
        config.jumpVerticalSpeed, sizeScale, config.jumpHeightSizeExponent);
}

float movementScaledJumpVelocity(float jumpVelocity, float sizeScale, float jumpHeightExponent)
{
    return jumpVelocity * movementSizeScaleFactor(sizeScale, jumpHeightExponent);
}

bool movementHasMoveInput(glm::vec2 axes, float epsilon)
{
    return axes.x * axes.x + axes.y * axes.y > epsilon * epsilon;
}

float movementApplyGravityZ(float velocityZ,
                            float gravityZ,
                            float maximumFallSpeed,
                            float dt)
{
    float nextZ = velocityZ + gravityZ * dt;
    return std::max(nextZ, -maximumFallSpeed);
}

glm::vec2 movementWalkVelocityXY(glm::vec2 currentVelocity,
                                 glm::vec2 wishMoveXY,
                                 bool onGround,
                                 float sizeScale,
                                 const MovementConfig& config)
{
    const float wishLen = std::sqrt(wishMoveXY.x * wishMoveXY.x +
                                   wishMoveXY.y * wishMoveXY.y);
    if (wishLen < 0.0001f)
        return currentVelocity;

    const glm::vec2 wishDir = wishMoveXY / wishLen;
    const float speed =
        onGround ? movementScaledGroundSpeed(config, sizeScale)
                 : movementScaledAirSpeed(config, sizeScale);
    return wishDir * speed;
}

void movementApplyWalkVelocity(glm::vec3& baseVelocity,
                               glm::vec2 wishMoveXY,
                               bool onGround,
                               float sizeScale,
                               float groundSpeed,
                               float airSpeed,
                               float movementSpeedSizeExponent)
{
    MovementConfig config;
    config.groundSpeed = groundSpeed;
    config.airSpeed = airSpeed;
    config.movementSpeedSizeExponent = movementSpeedSizeExponent;
    const glm::vec2 next =
        movementWalkVelocityXY(glm::vec2(baseVelocity), wishMoveXY, onGround, sizeScale, config);
    baseVelocity.x = next.x;
    baseVelocity.y = next.y;
}

glm::vec2 movementApplyBaseFrictionXY(glm::vec2 velocityXY,
                                      bool onGround,
                                      bool hasMoveInput,
                                      float sizeScale,
                                      float frictionOverride,
                                      const MovementConfig& config,
                                      float dt)
{
    return movementApplyBaseFrictionXY(velocityXY,
                                      onGround,
                                      hasMoveInput,
                                      sizeScale,
                                      frictionOverride,
                                      config.groundFrictionAmount,
                                      config.airFrictionAmount,
                                      config.frictionSizeExponent,
                                      config.almostZeroSpeed,
                                      dt);
}

glm::vec2 movementApplyBaseFrictionXY(glm::vec2 velocityXY,
                                      bool onGround,
                                      bool hasMoveInput,
                                      float sizeScale,
                                      float frictionOverride,
                                      float groundFrictionAmount,
                                      float airFrictionAmount,
                                      float frictionSizeExponent,
                                      float almostZeroSpeed,
                                      float dt)
{
    if (onGround && hasMoveInput)
        return velocityXY;

    const float frictionMul = std::clamp(frictionOverride, 0.0f, 1.0f);
    const float frictionAmount =
        (onGround ? groundFrictionAmount : airFrictionAmount) *
        frictionMul *
        movementSizeScaleFactor(sizeScale, frictionSizeExponent);
    velocityXY *= expDecay(frictionAmount, dt);

    if (glm::length(velocityXY) < almostZeroSpeed)
        return glm::vec2(0.0f);
    return velocityXY;
}

void movementDecayAndClampExternalImpulse(glm::vec3& externalImpulse,
                                          const MovementConfig& config,
                                          float frictionOverride,
                                          float dt)
{
    movementDecayAndClampExternalImpulse(externalImpulse,
                                         config.externalImpulseDecay,
                                         frictionOverride,
                                         config.maximumExternalImpulseSpeed,
                                         config.almostZeroSpeed,
                                         dt);
}

void movementDecayAndClampExternalImpulse(glm::vec3& externalImpulse,
                                          float externalImpulseDecay,
                                          float frictionOverride,
                                          float maximumExternalImpulseSpeed,
                                          float almostZeroSpeed,
                                          float dt)
{
    const float frictionMul = std::clamp(frictionOverride, 0.0f, 1.0f);
    externalImpulse *= expDecay(externalImpulseDecay * frictionMul, dt);

    if (glm::length(externalImpulse) < almostZeroSpeed) {
        externalImpulse = glm::vec3(0.0f);
        return;
    }

    glm::vec2 impulseXY(externalImpulse.x, externalImpulse.y);
    const float impulseSpeed = glm::length(impulseXY);
    if (impulseSpeed > maximumExternalImpulseSpeed && impulseSpeed > 0.0f) {
        impulseXY *= maximumExternalImpulseSpeed / impulseSpeed;
        externalImpulse.x = impulseXY.x;
        externalImpulse.y = impulseXY.y;
    }
}

bool movementCanGroundJump(const MovementState& state)
{
    return state.ground.onGround || state.jump.coyoteTimerSeconds > 0.0f;
}

bool movementCanAirJump(const MovementState& state)
{
    return state.jump.airJumpsLeft > 0 && state.jump.airJumpArmed;
}

void applyBasicGravity(MovementState& state,
                       const MovementConfig& config,
                       float fixedDt)
{
    const float dt = movementClampStepDelta(fixedDt, config);
    state.baseVelocity.z = movementApplyGravityZ(
        state.baseVelocity.z, config.gravityZ, config.maximumFallSpeed, dt);
}

void applyBasicExternalImpulseControl(MovementState& state,
                                      const MovementCommand& command)
{
    if (!command.movementDirectionPressed &&
        !command.dashPressed &&
        !command.freezeHeld) {
        return;
    }

    const float upZ = state.externalImpulse.z > 0.0f ? state.externalImpulse.z : 0.0f;
    state.externalImpulse = glm::vec3(0.0f);
    state.externalImpulse.z = upZ;
}

void applyBasicWalk(MovementState& state,
                    const MovementCommand& command,
                    const MovementConfig& config)
{
    const glm::vec2 next =
        movementWalkVelocityXY(glm::vec2(state.baseVelocity),
                               command.moveAxes,
                               state.ground.onGround,
                               state.sizeScale,
                               config);
    state.baseVelocity.x = next.x;
    state.baseVelocity.y = next.y;
}

void applyBasicJump(MovementState& state,
                    const MovementCommand& command,
                    const MovementConfig& config,
                    float fixedDt,
                    MovementStepEvents* events)
{
    const float dt = movementClampStepDelta(fixedDt, config);
    state.jump.jumpIntentTimerSeconds =
        std::max(0.0f, state.jump.jumpIntentTimerSeconds - dt);
    state.jump.coyoteTimerSeconds =
        std::max(0.0f, state.jump.coyoteTimerSeconds - dt);

    if (state.ground.onGround)
        state.jump.coyoteTimerSeconds = config.coyoteSeconds;

    if (command.jumpHeld)
        state.jump.jumpIntentTimerSeconds = config.jumpBufferSeconds;

    const bool jumpPressedThisFrame =
        command.jumpPressed ||
        (command.jumpHeld && !state.jump.jumpHeldPreviously);
    if (jumpPressedThisFrame)
        state.jump.jumpIntentTimerSeconds = config.jumpBufferSeconds;

    const bool jumpReleased =
        !command.jumpHeld && state.jump.jumpHeldPreviously;
    if (jumpReleased) {
        state.jump.airJumpArmed = true;
        state.jump.airJumpLocked = false;
        state.jump.jumpIntentTimerSeconds = 0.0f;
    }

    state.jump.jumpHeldPreviously = command.jumpHeld;

    const bool wantsJump = state.jump.jumpIntentTimerSeconds > 0.0f;
    if (!wantsJump)
        return;

    if (movementCanGroundJump(state)) {
        state.dash.dashAvailable = true;
        state.baseVelocity.z = movementScaledJumpVelocity(config, state.sizeScale);
        state.ground.onGround = false;
        state.jump.coyoteTimerSeconds = 0.0f;
        state.jump.jumpIntentTimerSeconds = 0.0f;
        state.jump.airJumpsLeft = config.maximumAirJumps;
        state.jump.airJumpLocked = true;
        state.jump.airJumpArmed = false;
        state.jump.didGroundJump = true;
        if (events)
            events->didGroundJump = true;
        return;
    }

    if (movementCanAirJump(state)) {
        state.baseVelocity.z = movementScaledJumpVelocity(config, state.sizeScale);
        --state.jump.airJumpsLeft;
        state.jump.airJumpArmed = false;
        state.jump.airJumpLocked = true;
        state.jump.jumpIntentTimerSeconds = 0.0f;
        state.jump.didAirJump = true;
        if (events)
            events->didAirJump = true;
    }
}

void applyBasicFriction(MovementState& state,
                        const MovementConfig& config,
                        float fixedDt)
{
    const float dt = movementClampStepDelta(fixedDt, config);
    const bool hasMoveInput = movementHasMoveInput(state.lastInputMoveAxes);
    const float frictionOverride = std::clamp(state.dash.frictionOverride, 0.0f, 1.0f);

    const glm::vec2 frictioned = movementApplyBaseFrictionXY(
        glm::vec2(state.baseVelocity),
        state.ground.stableOnGround,
        hasMoveInput,
        state.sizeScale,
        frictionOverride,
        config,
        dt);
    state.baseVelocity.x = frictioned.x;
    state.baseVelocity.y = frictioned.y;

    movementDecayAndClampExternalImpulse(
        state.externalImpulse, config, frictionOverride, dt);
}

void applyPreCollisionBasicMovement(MovementState& state,
                                    const MovementCommand& command,
                                    const MovementConfig& config,
                                    float fixedDt)
{
    state.lastInputMoveAxes = movementClampUnitOrZero(command.moveAxes);
    resetBasicOneTickState(state);
    applyBasicGravity(state, config, fixedDt);
}

MovementStepResult applyPostCollisionBasicMovement(MovementState& state,
                                                   const MovementCommand& command,
                                                   const MovementConfig& config,
                                                   const MovementCollisionFeedback& collision,
                                                   float fixedDt)
{
    MovementStepEvents events;
    const float dt = movementClampStepDelta(fixedDt, config);
    const bool previousOnGround = state.ground.onGround;
    const bool previousStableOnGround = state.ground.stableOnGround;
    const float previousAirborneSeconds = state.ground.airborneTimerSeconds;

    state.ground.onGround = collision.onGround;
    events.touchedGround = collision.onGround;
    events.leftGround = previousOnGround && !collision.onGround;

    if (collision.onGround)
        state.ground.groundLostTimerSeconds = 0.0f;
    else
        state.ground.groundLostTimerSeconds += dt;

    state.ground.stableOnGround =
        collision.onGround ||
        state.ground.groundLostTimerSeconds < config.stableGroundGraceSeconds;
    state.ground.hasWorldContact = collision.hasWorldContact;
    state.ground.realWorldContactThisFrame = collision.realWorldContactThisFrame;
    if (movementIsFinite(collision.groundNormal))
        state.ground.groundNormal = collision.groundNormal;

    if (!collision.onGround && command.movementDirectionPressed) {
        if (state.dash.dashMovementTicks < 99)
            ++state.dash.dashMovementTicks;
    } else {
        state.dash.dashMovementTicks = 0;
    }

    applyBasicExternalImpulseControl(state, command);

    if (command.movementDirectionPressed && state.dash.frictionOverride >= 1.0f)
        applyBasicWalk(state, command, config);

    applyBasicJump(state, command, config, dt, &events);
    applyBasicFrictionOverrideRecovery(state, command);
    applyGroundedResourceSafetyReset(state, config, collision.onGround);
    applyBasicFriction(state, config, dt);
    applyBasicLandingTimers(
        state, config, previousStableOnGround, previousAirborneSeconds, dt, events);

    events.didGroundJump = state.jump.didGroundJump;
    events.didAirJump = state.jump.didAirJump;
    events.didLand = state.ground.didLand;

    MovementStepResult result;
    result.state = state;
    result.events = events;
    return result;
}

MovementStepResult simulateBasicMovementStep(MovementState& state,
                                             const MovementCommand& command,
                                             const MovementConfig& config,
                                             const MovementCollisionFeedback& collision,
                                             float fixedDt)
{
    applyPreCollisionBasicMovement(state, command, config, fixedDt);
    return applyPostCollisionBasicMovement(state, command, config, collision, fixedDt);
}
