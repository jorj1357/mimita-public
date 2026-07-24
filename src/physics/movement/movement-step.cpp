// 07 21 2026, 17 25
/* purpose
* Implements the shared movement kernel, pure formula helpers, and contact reset consumer.
* Preserves local collision boundaries by splitting pre and post collision phases.
* Reuses one formula for walking, gravity, jump, impulse, dash, down dash, freeze, and touch reset.
* Does NOT perform collision sweeps, packet handling, rendering, audio, or authority decisions.
* Does NOT generate collision facts, migrate networking, or own server prediction history.
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

uint8_t saturatingAddContactCount(uint8_t value, uint16_t add)
{
    const uint16_t next = static_cast<uint16_t>(value) + add;
    return static_cast<uint8_t>(std::min<uint16_t>(next, 255));
}

MovementAbilityResetResult restoreTouchAvailability(MovementState& state,
                                                    const MovementConfig* config,
                                                    bool includeAirJump)
{
    MovementAbilityResetResult result;

    if (includeAirJump && config) {
        result.airJumpRestored =
            state.jump.airJumpsLeft != config->maximumAirJumps ||
            !state.jump.airJumpArmed ||
            state.jump.airJumpLocked;
        state.jump.airJumpsLeft = config->maximumAirJumps;
        state.jump.airJumpArmed = true;
        state.jump.airJumpLocked = false;
    }

    result.dashRestored = !state.dash.dashAvailable;
    state.dash.dashAvailable = true;

    result.groundReturnRestored = !state.groundReturn.available;
    state.groundReturn.available = true;

    result.downDashRestored = !state.downDash.available;
    state.downDash.available = true;

    result.freezeRestored = !state.freeze.available;
    state.freeze.available = true;

    result.anyRestored =
        result.airJumpRestored ||
        result.dashRestored ||
        result.downDashRestored ||
        result.freezeRestored ||
        result.groundReturnRestored;
    return result;
}

void markContactEventKind(MovementStepEvents& events, MovementContactKind kind)
{
    if (kind == MovementContactKind::Ground ||
        kind == MovementContactKind::Step)
        events.touchedGround = true;
    else if (kind == MovementContactKind::Wall)
        events.touchedWall = true;
    else if (kind == MovementContactKind::Ceiling)
        events.touchedCeiling = true;
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

float movementDashImpulse(const MovementConfig& config)
{
    return positiveOrDefault(
        config.dashHorizontalImpulse,
        positiveOrDefault(config.airDashImpulse, 50.0f));
}

bool movementHasMoveInput(glm::vec2 axes, float epsilon)
{
    return axes.x * axes.x + axes.y * axes.y > epsilon * epsilon;
}

glm::vec2 movementHorizontalForwardFromYaw(float yawDegrees)
{
    if (!std::isfinite(yawDegrees))
        return glm::vec2(1.0f, 0.0f);

    constexpr float kPi = 3.14159265358979323846f;
    const float radians = yawDegrees * (kPi / 180.0f);
    return glm::vec2(std::cos(radians), std::sin(radians));
}

glm::vec2 movementDashDirection(const MovementCommand& command, float epsilon)
{
    const glm::vec2 move = movementNormalizeDirectionOrZero(command.moveAxes, epsilon);
    if (movementHasMoveInput(move, epsilon))
        return move;

    if (movementIsFinite(command.horizontalCameraForward)) {
        const glm::vec2 cameraForward(
            command.horizontalCameraForward.x,
            command.horizontalCameraForward.y);
        const glm::vec2 normalizedForward =
            movementNormalizeDirectionOrZero(cameraForward, epsilon);
        if (movementHasMoveInput(normalizedForward, epsilon))
            return normalizedForward;
    }

    return movementHorizontalForwardFromYaw(command.lookYaw);
}

bool movementDirectionsEquivalent(glm::vec2 a, glm::vec2 b, float epsilon)
{
    const glm::vec2 normalizedA = movementNormalizeDirectionOrZero(a, epsilon);
    const glm::vec2 normalizedB = movementNormalizeDirectionOrZero(b, epsilon);
    const bool aPressed = movementHasMoveInput(normalizedA, epsilon);
    const bool bPressed = movementHasMoveInput(normalizedB, epsilon);
    if (!aPressed || !bPressed)
        return aPressed == bPressed;

    const glm::vec2 delta = normalizedA - normalizedB;
    return delta.x * delta.x + delta.y * delta.y <= epsilon * epsilon;
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

float freezeHorizontalPassThrough(const MovementFreezeState& freeze,
                                  const MovementConfig& config)
{
    if (!freeze.active)
        return 1.0f;
    return movementFreezeHorizontalPassThrough(freeze.timerSeconds,
                                               config.freezeDurationSeconds,
                                               config.freezeCurveExponent);
}

glm::vec2 effectiveHorizontalVelocity(const MovementState& state,
                                      float horizontalPassThrough)
{
    const glm::vec2 base(state.baseVelocity.x, state.baseVelocity.y);
    const glm::vec2 impulse(state.externalImpulse.x, state.externalImpulse.y);
    return (base + impulse) * horizontalPassThrough;
}

MovementVelocityView movementVelocityViewForCollision(const MovementState& state,
                                                      const MovementConfig& config)
{
    MovementVelocityView view;
    view.horizontalPassThrough = freezeHorizontalPassThrough(state.freeze, config);
    view.effectiveBaseVelocity = state.baseVelocity;
    view.effectiveExternalImpulse = state.externalImpulse;
    view.effectiveBaseVelocity.x *= view.horizontalPassThrough;
    view.effectiveBaseVelocity.y *= view.horizontalPassThrough;
    view.effectiveBaseVelocity.z *= view.horizontalPassThrough;
    view.effectiveExternalImpulse.x *= view.horizontalPassThrough;
    view.effectiveExternalImpulse.y *= view.horizontalPassThrough;
    view.effectiveExternalImpulse.z *= view.horizontalPassThrough;
    return view;
}

glm::vec3 calculateEffectiveMovementVelocity(const MovementState& state,
                                             const MovementConfig& config)
{
    const MovementVelocityView view = movementVelocityViewForCollision(state, config);
    return view.effectiveBaseVelocity + view.effectiveExternalImpulse;
}

MovementContactKind classifyMovementContactKindFromNormal(const glm::vec3& normal,
                                                          const MovementConfig& config,
                                                          bool grounded,
                                                          bool step)
{
    if (step)
        return MovementContactKind::Step;
    if (!movementIsFinite(normal))
        return MovementContactKind::StaticWorld;

    const float walkableDot =
        config.walkableSlopeDot > 0.0f ? config.walkableSlopeDot : 0.7f;
    if (grounded && normal.z > 0.0f)
        return MovementContactKind::Ground;
    if (normal.z > walkableDot)
        return MovementContactKind::Ground;
    if (normal.z > 0.0f)
        return MovementContactKind::Slope;
    if (normal.z < -walkableDot)
        return MovementContactKind::Ceiling;
    return MovementContactKind::Wall;
}

MovementContact makeStaticWorldMovementContact(MovementContactKind kind,
                                               uint64_t simulationTick,
                                               MovementLifecycleIdentity targetLifecycle,
                                               const glm::vec3& point,
                                               const glm::vec3& normal,
                                               uint32_t surfaceId,
                                               float penetrationDepth,
                                               bool resetsAbilities)
{
    MovementContact contact;
    contact.kind = kind;
    contact.source = MovementContactSource::StaticWorld;
    contact.targetLifecycle = targetLifecycle;
    contact.surfaceId = surfaceId;
    contact.simulationTick = simulationTick;
    contact.point = movementIsFinite(point) ? point : glm::vec3(0.0f);
    contact.normal =
        movementIsFinite(normal) ? normal : glm::vec3(0.0f, 0.0f, 1.0f);
    contact.penetrationDepth = penetrationDepth;
    contact.strength = penetrationDepth;
    contact.resetsAbilities = resetsAbilities;
    return contact;
}

MovementContact makeEntityMovementContact(MovementContactKind kind,
                                          MovementContactSource source,
                                          uint32_t sourceEntityId,
                                          uint64_t contactId,
                                          uint64_t sourceEventId,
                                          uint64_t simulationTick,
                                          MovementLifecycleIdentity targetLifecycle,
                                          const glm::vec3& point,
                                          const glm::vec3& normal,
                                          float strength,
                                          bool resetsAbilities)
{
    MovementContact contact;
    contact.kind = kind;
    contact.source = source;
    contact.targetLifecycle = targetLifecycle;
    contact.contactId = contactId;
    contact.sourceEventId = sourceEventId;
    contact.sourceEntityId = sourceEntityId;
    contact.simulationTick = simulationTick;
    contact.point = movementIsFinite(point) ? point : glm::vec3(0.0f);
    contact.normal =
        movementIsFinite(normal) ? normal : glm::vec3(0.0f, 0.0f, 1.0f);
    contact.strength = strength;
    contact.resetsAbilities = resetsAbilities;
    return contact;
}

MovementContact makeProjectileMovementContact(uint32_t projectileId,
                                              uint64_t eventId,
                                              uint64_t simulationTick,
                                              MovementLifecycleIdentity targetLifecycle,
                                              const glm::vec3& point,
                                              const glm::vec3& normal,
                                              MovementContactSource source)
{
    return makeEntityMovementContact(MovementContactKind::Projectile,
                                     source,
                                     projectileId,
                                     0,
                                     eventId,
                                     simulationTick,
                                     targetLifecycle,
                                     point,
                                     normal,
                                     0.0f,
                                     true);
}

MovementContact makeExplosionMovementContact(uint64_t explosionEventId,
                                             uint32_t sourceEntityId,
                                             uint64_t simulationTick,
                                             MovementLifecycleIdentity targetLifecycle,
                                             const glm::vec3& center,
                                             float strength)
{
    return makeEntityMovementContact(MovementContactKind::Explosion,
                                     MovementContactSource::Explosion,
                                     sourceEntityId,
                                     0,
                                     explosionEventId,
                                     simulationTick,
                                     targetLifecycle,
                                     center,
                                     glm::vec3(0.0f, 0.0f, 1.0f),
                                     strength,
                                     true);
}

MovementAbilityResetResult resetTouchAbilities(MovementState& state,
                                               const MovementConfig& config)
{
    return restoreTouchAvailability(state, &config, true);
}

MovementContactConsumeResult consumeMovementContacts(
    MovementState& state,
    const MovementConfig& config,
    const MovementContactSet& contacts,
    MovementContactHistory& history,
    MovementStepEvents& events)
{
    MovementContactConsumeResult result;
    result.contactOverflowCount = contacts.overflowCount;
    result.duplicateContactCount =
        static_cast<uint8_t>(std::min<uint16_t>(contacts.duplicateCount, 255));

    history.ensureLifecycle(state.lifecycle);

    bool acceptedResetContact = false;
    for (const MovementContact& contact : contacts) {
        events.contacts.addDeduplicated(contact);
        markContactEventKind(events, contact.kind);

        if (!movementContactMatchesLifecycle(contact, state.lifecycle)) {
            result.ignoredLifecycleCount =
                saturatingAddContactCount(result.ignoredLifecycleCount, 1);
            continue;
        }
        if (!contact.resetsAbilities)
            continue;

        result.consumedAnyContact = true;
        result.qualifyingContactCount =
            saturatingAddContactCount(result.qualifyingContactCount, 1);

        if (history.containsStable(contact)) {
            result.duplicateContactCount =
                saturatingAddContactCount(result.duplicateContactCount, 1);
            continue;
        }

        history.recordStable(contact);
        acceptedResetContact = true;
    }

    result.contactOverflowCount =
        static_cast<uint16_t>(result.contactOverflowCount + events.contacts.overflowCount);
    result.duplicateContactCount = saturatingAddContactCount(
        result.duplicateContactCount, events.contacts.duplicateCount);

    events.qualifyingContactCount = saturatingAddContactCount(
        events.qualifyingContactCount, result.qualifyingContactCount);
    events.dedupedContactCount = saturatingAddContactCount(
        events.dedupedContactCount, result.duplicateContactCount);
    events.contactOverflowCount =
        static_cast<uint16_t>(events.contactOverflowCount + result.contactOverflowCount);

    if (!acceptedResetContact)
        return result;

    result.appliedReset = true;
    result.reset = resetTouchAbilities(state, config);
    if (!result.reset.anyRestored)
        return result;

    events.abilitiesReset = true;
    events.resetAirControlAbilities = true;
    events.airJumpRestored = result.reset.airJumpRestored;
    events.dashRestored = result.reset.dashRestored;
    events.downDashRestored = result.reset.downDashRestored;
    events.freezeRestored = result.reset.freezeRestored;
    events.groundReturnRestored = result.reset.groundReturnRestored;
    return result;
}

void restoreSpecialAbilityAvailability(MovementState& state)
{
    restoreTouchAvailability(state, nullptr, false);
}

void resetSpecialMovementLifecycleState(MovementState& state)
{
    state.dash = MovementDashState{};
    state.downDash = MovementDownDashState{};
    state.freeze = MovementFreezeState{};
    state.groundReturn = MovementGroundReturnState{};
    state.dashMomentumProtection = MovementDashMomentumProtectionState{};
    state.contactHistory.clear();
}

bool shouldWalkingOverwriteDashMomentum(const MovementState& state,
                                        const MovementCommand& command,
                                        float epsilon)
{
    if (!state.dashMomentumProtection.active)
        return true;
    if (!command.movementDirectionPressed)
        return false;
    if (command.movementDirectionFreshPressed || command.movementDirectionChanged)
        return true;
    if (state.dashMomentumProtection.usedCameraForwardFallback)
        return true;
    return !movementDirectionsEquivalent(command.moveAxes,
                                         state.dashMomentumProtection.protectedMoveAxes,
                                         epsilon);
}

void updateDashMomentumProtectionForWalk(MovementState& state,
                                         const MovementCommand& command,
                                         float epsilon)
{
    if (!state.dashMomentumProtection.active)
        return;

    if (!command.movementDirectionPressed) {
        if (command.movementDirectionReleased)
            ++state.dashMomentumProtection.movementInputGeneration;
        return;
    }

    if (!shouldWalkingOverwriteDashMomentum(state, command, epsilon))
        return;

    state.dashMomentumProtection.active = false;
    state.dashMomentumProtection.protectedMoveAxes = glm::vec2(0.0f);
    state.dashMomentumProtection.usedCameraForwardFallback = false;
    ++state.dashMomentumProtection.movementInputGeneration;
}

bool tryActivateDash(MovementState& state,
                     const MovementCommand& command,
                     const MovementConfig& config,
                     MovementStepEvents& events)
{
    if (!command.dashPressed)
        return false;
    if (!state.dash.dashAvailable)
        return false;

    const float passThrough = freezeHorizontalPassThrough(state.freeze, config);
    if (state.freeze.active && passThrough <= config.freezeDashMinimumPassThrough) {
        events.dashRejectedByFreeze = true;
        return false;
    }

    const glm::vec2 direction = movementDashDirection(command);
    if (!movementHasMoveInput(direction, MOVEMENT_INPUT_EPSILON))
        return false;

    const float impulse = movementDashImpulse(config);
    state.baseVelocity.x += direction.x * impulse;
    state.baseVelocity.y += direction.y * impulse;
    state.dash.dashAvailable = false;
    state.dash.didDash = true;
    state.dash.frictionOverride = 1.0f;
    state.dash.tickPerfectDash = false;
    state.jump.airJumpsLeft = 0;

    const bool usedMoveInput = movementHasMoveInput(command.moveAxes, MOVEMENT_INPUT_EPSILON);
    state.dashMomentumProtection.active = true;
    state.dashMomentumProtection.usedCameraForwardFallback = !usedMoveInput;
    state.dashMomentumProtection.protectedMoveAxes =
        usedMoveInput ? movementNormalizeDirectionOrZero(command.moveAxes)
                      : glm::vec2(0.0f);
    ++state.dashMomentumProtection.movementInputGeneration;

    events.didDash = true;
    return true;
}

bool tryActivateDownDash(MovementState& state,
                         const MovementCommand& command,
                         const MovementConfig& config,
                         MovementStepEvents& events)
{
    if (!command.downDashPressed)
        return false;
    if (!state.downDash.available)
        return false;

    state.baseVelocity.z = config.downDashVerticalSpeed;
    state.downDash.available = false;
    state.downDash.didDownDash = true;
    events.didDownDash = true;
    return true;
}

void updateFreeze(MovementState& state,
                  const MovementCommand& command,
                  const MovementConfig& config,
                  float fixedDt,
                  MovementStepEvents& events)
{
    const float dt = movementClampStepDelta(fixedDt, config);
    const bool freezePressed =
        command.freezePressed ||
        (command.freezeHeld && !state.freeze.heldPreviously);
    const bool freezeReleased =
        !command.freezeHeld && state.freeze.heldPreviously;

    bool startedThisTick = false;
    if (freezePressed && state.freeze.available) {
        state.baseVelocity.x = 0.0f;
        state.baseVelocity.y = 0.0f;
        state.baseVelocity.z = 0.0f;
        state.freeze.active = true;
        state.freeze.available = false;
        state.freeze.timerSeconds = 0.0f;
        state.freeze.didFreeze = true;
        events.didFreeze = true;
        events.freezeStarted = true;
        startedThisTick = true;
    }

    if (freezeReleased && state.freeze.active) {
        state.freeze.active = false;
        events.freezeEnded = true;
    }

    state.freeze.heldPreviously = command.freezeHeld;

    if (!state.freeze.active || !command.freezeHeld || startedThisTick)
        return;

    state.freeze.timerSeconds += dt;
    if (config.freezeDurationSeconds > 0.0f)
        state.freeze.timerSeconds =
            std::min(state.freeze.timerSeconds, config.freezeDurationSeconds);
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
        !command.dashPressed) {
        return;
    }

    state.externalImpulse = glm::vec3(0.0f);
}

static void applySpecialExternalImpulseControl(MovementState& state,
                                               const MovementCommand& command)
{
    if (!command.movementDirectionPressed)
        return;
    if (command.dashPressed ||
        command.downDashPressed ||
        command.freezeHeld ||
        state.dashMomentumProtection.active) {
        return;
    }

    applyBasicExternalImpulseControl(state, command);
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

void applySpecialMovementPreCollision(MovementState& state,
                                      const MovementCommand& command,
                                      const MovementConfig& config,
                                      float fixedDt,
                                      MovementStepEvents& events)
{
    updateFreeze(state, command, config, fixedDt, events);
    tryActivateDownDash(state, command, config, events);
}

void applySpecialMovementPostCollision(MovementState& state,
                                       const MovementCommand& command,
                                       const MovementConfig& config,
                                       float fixedDt,
                                       MovementStepEvents& events)
{
    (void)fixedDt;
    tryActivateDash(state, command, config, events);
}

static MovementStepResult applyPostCollisionMovementInternal(
    MovementState& state,
    const MovementCommand& command,
    const MovementConfig& config,
    const MovementCollisionFeedback& collision,
    float fixedDt,
    bool specialMovementEnabled,
    MovementStepEvents events)
{
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

    MovementContactSet contacts = collision.contacts;
    if (contacts.empty() && collision.onGround) {
        contacts.addDeduplicated(makeStaticWorldMovementContact(
            MovementContactKind::Ground,
            collision.simulationTick,
            state.lifecycle,
            state.position,
            state.ground.groundNormal,
            0,
            0.0f));
    }
    consumeMovementContacts(
        state, config, contacts, state.contactHistory, events);

    if (!collision.onGround && command.movementDirectionPressed) {
        if (state.dash.dashMovementTicks < 99)
            ++state.dash.dashMovementTicks;
    } else {
        state.dash.dashMovementTicks = 0;
    }

    if (specialMovementEnabled) {
        updateDashMomentumProtectionForWalk(state, command);
        applySpecialExternalImpulseControl(state, command);
    } else {
        applyBasicExternalImpulseControl(state, command);
    }

    const bool walkingMayOverwriteDash =
        !specialMovementEnabled ||
        shouldWalkingOverwriteDashMomentum(state, command);
    if (command.movementDirectionPressed &&
        state.dash.frictionOverride >= 1.0f &&
        walkingMayOverwriteDash) {
        applyBasicWalk(state, command, config);
    }

    if (specialMovementEnabled)
        applySpecialMovementPostCollision(state, command, config, dt, events);
    applyBasicJump(state, command, config, dt, &events);
    applyBasicFrictionOverrideRecovery(state, command);
    applyBasicFriction(state, config, dt);
    applyBasicLandingTimers(
        state, config, previousStableOnGround, previousAirborneSeconds, dt, events);

    events.didGroundJump = state.jump.didGroundJump;
    events.didAirJump = state.jump.didAirJump;
    events.didLand = state.ground.didLand;
    events.didDash = state.dash.didDash;
    events.didDownDash = state.downDash.didDownDash;
    events.didFreeze = state.freeze.didFreeze;

    MovementStepResult result;
    result.state = state;
    result.events = events;
    return result;
}

MovementStepResult applyPostCollisionBasicMovement(MovementState& state,
                                                   const MovementCommand& command,
                                                   const MovementConfig& config,
                                                   const MovementCollisionFeedback& collision,
                                                   float fixedDt)
{
    return applyPostCollisionMovementInternal(
        state, command, config, collision, fixedDt, false, MovementStepEvents{});
}

MovementStepResult applyPostCollisionMovementWithSpecials(
    MovementState& state,
    const MovementCommand& command,
    const MovementConfig& config,
    const MovementCollisionFeedback& collision,
    float fixedDt,
    MovementStepEvents preCollisionEvents)
{
    return applyPostCollisionMovementInternal(
        state, command, config, collision, fixedDt, true, preCollisionEvents);
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

MovementStepResult simulateMovementStepWithSpecials(MovementState& state,
                                                    const MovementCommand& command,
                                                    const MovementConfig& config,
                                                    const MovementCollisionFeedback& collision,
                                                    float fixedDt)
{
    applyPreCollisionBasicMovement(state, command, config, fixedDt);
    MovementStepEvents preCollisionEvents;
    applySpecialMovementPreCollision(state, command, config, fixedDt, preCollisionEvents);
    return applyPostCollisionMovementWithSpecials(
        state, command, config, collision, fixedDt, preCollisionEvents);
}
