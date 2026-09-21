// 08 15 2026, 20 52
/* purpose
* Implements the shared movement kernel, pure formula helpers, and contact reset consumer.
* Preserves local collision boundaries by splitting pre and post collision phases.
* Reuses one formula for walking, gravity, jump, impulse, dash, down dash, freeze, and touch reset.
* Does NOT perform collision sweeps, packet handling, rendering, audio, or authority decisions.
* Does NOT generate collision facts, migrate networking, or own server prediction history.
* Does NOT own presentation state or mutate Player directly.
*/

#include "physics/movement/movement-step.h"
#include "hot-reload/hot-movement-policy.h"
#include "live-code/live-behavior.h"

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

// Shortest signed angular difference in degrees, in [-180, 180).
float wrapSignedDegrees(float value)
{
    value = std::fmod(value + 180.0f, 360.0f);
    if (value < 0.0f)
        value += 360.0f;
    return value - 180.0f;
}

float shortestSignedAngleDegrees(float from, float to)
{
    return wrapSignedDegrees(to - from);
}

float signedAngleDegrees(glm::vec2 a, glm::vec2 b)
{
    a = movementNormalizeDirectionOrZero(a);
    b = movementNormalizeDirectionOrZero(b);
    const float dot = glm::clamp(glm::dot(a, b), -1.0f, 1.0f);
    const float angle = std::acos(dot);
    const float cross = a.x * b.y - a.y * b.x;
    return (cross >= 0.0f ? 1.0f : -1.0f) * glm::degrees(angle);
}

float cross2D(glm::vec2 a, glm::vec2 b)
{
    return a.x * b.y - a.y * b.x;
}

// Returns vec capped to maxLength (direction preserved).
glm::vec2 clampVecLength(glm::vec2 v, float maxLength)
{
    const float len = glm::length(v);
    if (len <= maxLength || len <= 0.0001f)
        return v;
    return v * (maxLength / len);
}

// Rotates unit vector a toward unit vector b by at most maxAngleRadians.
glm::vec2 rotateToward(glm::vec2 a, glm::vec2 b, float maxAngleRadians)
{
    a = glm::normalize(a);
    b = glm::normalize(b);
    const float dot = glm::clamp(glm::dot(a, b), -1.0f, 1.0f);
    const float angle = std::acos(dot);
    if (angle <= 1e-5f || angle <= maxAngleRadians)
        return b;
    const glm::vec2 perp = glm::normalize(b - a * dot);
    return a * std::cos(maxAngleRadians) + perp * std::sin(maxAngleRadians);
}

enum class AirStrafeRejection : uint8_t {
    None,
    Grounded,
    NoInput,
    CameraStationary,
    WishStationary,
    WrongTurnRelationship,
    BelowAngularTolerance,
    AtSpeedCap,
    FeatureDisabled
};

struct AirStrafeEvaluation {
    bool hasInput = false;
    bool cameraIsTurning = false;
    bool wishIsRotating = false;
    bool inputMatchesTurn = false;
    bool withinTolerance = true;
    bool validSteering = false;
    bool validSpeedGain = false;
    AirStrafeRejection rejection = AirStrafeRejection::None;
    float cameraYawDelta = 0.0f;
    float wishAngleDelta = 0.0f;
    // Signed wish-direction rotation this tick (0 on a fresh key press), used
    // as the steering scale: the mouse rotates the wish reference and the
    // velocity curves around it.
    float steerRotationDegrees = 0.0f;
    int turnSign = 0;
    int side = 0;
};

AirStrafeEvaluation evaluateAirStrafe(const MovementState& state,
                                      const MovementCommand& command,
                                      const MovementConfig& config)
{
    AirStrafeEvaluation ev;
    ev.hasInput = movementHasMoveInput(command.moveAxes);
    if (!ev.hasInput) {
        ev.rejection = AirStrafeRejection::NoInput;
        return ev;
    }

    ev.cameraYawDelta =
        shortestSignedAngleDegrees(state.previousYaw, command.lookYaw);
    ev.cameraIsTurning =
        std::fabs(ev.cameraYawDelta) >= config.minimumCameraYawDeltaDegrees;

    // Wish rotation requires a previous wish: a fresh key press is not a turn.
    const bool hadPrevInput = movementHasMoveInput(state.previousMoveAxes);
    const glm::vec2 prevWish =
        hadPrevInput ? movementNormalizeDirectionOrZero(state.previousMoveAxes)
                     : glm::vec2(0.0f);
    const glm::vec2 curWish = movementNormalizeDirectionOrZero(command.moveAxes);
    ev.wishAngleDelta = signedAngleDegrees(prevWish, curWish);
    ev.steerRotationDegrees = hadPrevInput ? ev.wishAngleDelta : 0.0f;
    ev.wishIsRotating =
        hadPrevInput &&
        std::fabs(ev.wishAngleDelta) >= config.minimumWishRotationDegrees;

    const glm::vec2 vel(state.baseVelocity.x, state.baseVelocity.y);
    const float speed = glm::length(vel);
    if (speed > 1e-4f) {
        const glm::vec2 velDir = vel / speed;
        ev.side = cross2D(velDir, curWish) > 0.0f ? 1 : (cross2D(velDir, curWish) < 0.0f ? -1 : 0);
        const float maxDot = std::sin(glm::radians(config.strafeAngularToleranceDegrees));
        ev.withinTolerance = std::fabs(glm::dot(velDir, curWish)) <= maxDot;
    }
    ev.turnSign = ev.cameraYawDelta > 0.0f ? 1 : (ev.cameraYawDelta < 0.0f ? -1 : 0);
    ev.inputMatchesTurn =
        ev.side != 0 && ev.turnSign != 0 && ev.side == ev.turnSign;

    // Speed-gain rule: any movement key + camera turn = eligible. Per-key,
    // turn-direction, and angular-tolerance discrimination are kept as debug
    // metrics only (inputMatchesTurn / withinTolerance / wishIsRotating).
    if (!ev.cameraIsTurning)
        ev.rejection = AirStrafeRejection::CameraStationary;

    ev.validSpeedGain = ev.hasInput && ev.cameraIsTurning;

    if (config.stationaryCameraInputMode == StationaryCameraInputMode::Strict)
        ev.validSteering = ev.cameraIsTurning;
    else
        ev.validSteering = ev.hasInput;
    return ev;
}

float airSoftCapMultiplier(float totalSpeed, const MovementConfig& config)
{
    if (!config.speedCapEnabled || config.bunnyHopSpeedCap <= 0.0f ||
        config.maximumBhopSpeedMode != MovementSpeedCapMode::Soft)
        return 1.0f;
    const float start = config.softCapStart > 0.0f
        ? config.softCapStart
        : config.bunnyHopSpeedCap * 0.6f;
    if (totalSpeed <= start)
        return 1.0f;
    if (totalSpeed >= config.bunnyHopSpeedCap)
        return 0.0f;
    const float t = (totalSpeed - start) / (config.bunnyHopSpeedCap - start);
    return 1.0f - t;
}

// Grounded: converge the player-controlled horizontal velocity toward the
// desired camera-relative velocity vector (no additive per-key forces).
void applyGroundControl(MovementState& state,
                        const glm::vec2& wish,
                        const MovementConfig& config,
                        float dt)
{
    glm::vec2 vel(state.baseVelocity.x, state.baseVelocity.y);
    const bool hasInput = movementHasMoveInput(wish);
    const glm::vec2 desired =
        hasInput ? movementNormalizeDirectionOrZero(wish) *
                       movementScaledGroundSpeed(config, state.sizeScale)
                 : glm::vec2(0.0f);

    // Preserve bhop momentum on the landing tick: if the player is about to
    // auto-jump, the ground controller must not trim the airborne speed.
    if (state.jump.jumpIntentTimerSeconds > 0.0f)
        return;

    const glm::vec2 delta = desired - vel;
    const float speed = glm::length(vel);
    const float desiredSpeed = glm::length(desired);

    float rate;
    const float angle = (speed > 1e-4f)
        ? glm::degrees(std::acos(glm::clamp(glm::dot(vel / speed,
              desiredSpeed > 1e-4f ? desired / desiredSpeed : glm::vec2(0.0f)),
              -1.0f, 1.0f)))
        : 0.0f;
    if (!hasInput) {
        rate = config.groundFrictionAmount;
    } else if (angle > 60.0f) {
        rate = config.groundDirectionChangeResponse;
    } else if (desiredSpeed > speed) {
        rate = config.groundAcceleration;
    } else {
        rate = config.groundDeceleration;
    }

    const float stepMax = std::max(rate * dt, 0.0f);
    vel += clampVecLength(delta, stepMax);
    state.baseVelocity.x = vel.x;
    state.baseVelocity.y = vel.y;
}

// Airborne, speed-preserving steering toward the wish direction.
void applyAirSteering(MovementState& state,
                      const glm::vec2& wishDir,
                      const MovementConfig& config,
                      float dt,
                      const AirStrafeEvaluation& ev)
{
    if (!ev.validSteering)
        return;
    glm::vec2 vel(state.baseVelocity.x, state.baseVelocity.y);
    const float speed = glm::length(vel);
    if (speed <= 1e-4f)
        return; // never create velocity from zero

    // Pole-like steering: the movement key selects the camera-relative wish
    // reference; the mouse rotates it. The velocity curves around that
    // reference by a fraction of the wish-direction rotation this tick.
    const float wishRot = std::fabs(ev.steerRotationDegrees);
    if (wishRot <= 1e-3f)
        return;
    float maxRotate = glm::radians(config.airSteeringResponse * wishRot);
    if (config.maximumSteeringDegreesPerSecond > 0.0f)
        maxRotate = std::min(maxRotate,
            glm::radians(config.maximumSteeringDegreesPerSecond) * dt);
    if (maxRotate <= 0.0f)
        return;
    const glm::vec2 steered = rotateToward(vel / speed, wishDir, maxRotate);
    state.baseVelocity.x = steered.x * speed;
    state.baseVelocity.y = steered.y * speed;
}

// Airborne speed gain, only for a valid strafe.
void applyAirSpeedGain(MovementState& state,
                       const glm::vec2& wishDir,
                       const MovementConfig& config,
                       float dt,
                       const AirStrafeEvaluation& ev,
                       float inputMagnitude)
{
    if (!ev.validSpeedGain)
        return;

    glm::vec2 vel(state.baseVelocity.x, state.baseVelocity.y);
    const float totalSpeed = glm::length(vel);

    float wishSpeed = config.airMaxWishspeed > 0.0f
        ? config.airMaxWishspeed
        : movementScaledAirSpeed(config, state.sizeScale);
    wishSpeed *= std::min(inputMagnitude, 1.0f);

    const float currentSpeedAlongWish = glm::dot(vel, wishDir);
    const float remaining = wishSpeed - currentSpeedAlongWish;
    if (remaining > 0.0f) {
        float gain = std::min(remaining, config.airAcceleration * wishSpeed * dt);
        if (config.maximumAccelerationPerTick > 0.0f)
            gain = std::min(gain, config.maximumAccelerationPerTick);
        gain *= airSoftCapMultiplier(totalSpeed, config);
        vel += wishDir * gain;
    }

    // Hard cap: clamp only the player-controlled bhop component.
    if (config.speedCapEnabled &&
        config.maximumBhopSpeedMode == MovementSpeedCapMode::Hard &&
        config.bunnyHopSpeedCap > 0.0f) {
        const float planar = glm::length(vel);
        if (planar > config.bunnyHopSpeedCap && planar > 0.0f)
            vel *= config.bunnyHopSpeedCap / planar;
    }

    state.baseVelocity.x = vel.x;
    state.baseVelocity.y = vel.y;
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
{    if (state.dash.frictionOverride >= 1.0f || state.dash.didDash)
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
                             float impactSpeed,
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
        state.ground.landingAirborneDurationSeconds = previousAirborneSeconds;
        state.ground.landingImpactSpeed = std::max(0.0f, impactSpeed);
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
    if (!config.dashEnabled)
        return false;
    if (!state.dash.dashAvailable)
        return false;

    const float passThrough = freezeHorizontalPassThrough(state.freeze, config);
    if (state.freeze.active && passThrough <= config.freezeDashMinimumPassThrough) {
        events.dashRejectedByFreeze = true;
        return false;
    }

    // Hot dash policy: a hot handler owns direction, impulse, and availability.
    {
        GameDashPolicyV1 d{};
        d.velocity[0] = state.baseVelocity.x;
        d.velocity[1] = state.baseVelocity.y;
        d.velocity[2] = state.baseVelocity.z;
        d.moveAxes[0] = command.moveAxes.x;
        d.moveAxes[1] = command.moveAxes.y;
        d.cameraForward[0] = command.horizontalCameraForward.x;
        d.cameraForward[1] = command.horizontalCameraForward.y;
        d.groundDashImpulse = positiveOrDefault(config.groundDashImpulse,
                                                movementDashImpulse(config));
        d.airDashImpulse = positiveOrDefault(config.airDashImpulse,
                                             movementDashImpulse(config));
        d.dashPressed = 1u;
        d.grounded = state.ground.onGround ? 1u : 0u;
        d.dashAvailable = 1u;
        d.dashEnabled = 1u;
        d.dashMovementTicks = state.dash.dashMovementTicks > 0
                                  ? static_cast<std::uint32_t>(state.dash.dashMovementTicks)
                                  : 0u;
        d.handled = 0;
        if (LiveBehavior::dispatchGameplayEvent64(GAME_EVENT_MOVEMENT_DASH, &d,
                                                  sizeof(d), 0, 0, 0) &&
            d.handled) {
            state.baseVelocity.x = d.outVelocity[0];
            state.baseVelocity.y = d.outVelocity[1];
            if (d.outDidDash == 0u)
                return false;
            state.dash.dashAvailable = false;
            state.dash.didDash = true;
            state.dash.frictionOverride = 1.0f;
            state.dash.tickPerfectDash = false;
            state.dash.dashGraceTimerSeconds = std::max(config.dashGraceSeconds, 0.0f);
            state.jump.airJumpsLeft = 0;
            const bool usedMoveInput = d.outUsedMoveInput != 0u;
            state.dashMomentumProtection.active = true;
            state.dashMomentumProtection.usedCameraForwardFallback = !usedMoveInput;
            state.dashMomentumProtection.protectedMoveAxes =
                usedMoveInput ? movementNormalizeDirectionOrZero(command.moveAxes)
                              : glm::vec2(0.0f);
            ++state.dashMomentumProtection.movementInputGeneration;
            events.didDash = true;
            return true;
        }
    }

    const glm::vec2 direction = movementDashDirection(command);
    if (!movementHasMoveInput(direction, MOVEMENT_INPUT_EPSILON))
        return false;

    // Ground dash uses ground_dash_impulse, air dash uses air_dash_impulse,
    // so the boost strength can differ between ground and air.
    const float impulse = state.ground.onGround
        ? positiveOrDefault(config.groundDashImpulse, movementDashImpulse(config))
        : positiveOrDefault(config.airDashImpulse, movementDashImpulse(config));
    state.baseVelocity.x += direction.x * impulse;
    state.baseVelocity.y += direction.y * impulse;
    state.dash.dashAvailable = false;
    state.dash.didDash = true;
    state.dash.frictionOverride = 1.0f;
    state.dash.tickPerfectDash = false;
    state.dash.dashGraceTimerSeconds = std::max(config.dashGraceSeconds, 0.0f);
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
    if (!config.downDashEnabled)
        return false;
    if (!state.downDash.available)
        return false;

    // Hot down-dash policy.
    GameDashPolicyV1 d{};
    d.velocity[0] = state.baseVelocity.x;
    d.velocity[1] = state.baseVelocity.y;
    d.velocity[2] = state.baseVelocity.z;
    d.downDashVerticalSpeed = config.downDashVerticalSpeed;
    d.downDashPressed = 1u;
    d.downDashEnabled = 1u;
    d.downDashAvailable = 1u;
    d.handled = 0;
    if (LiveBehavior::dispatchGameplayEvent64(GAME_EVENT_MOVEMENT_DASH, &d,
                                              sizeof(d), 0, 0, 0) &&
        d.handled) {
        if (d.outDidDownDash == 0u)
            return false;
        state.baseVelocity.z = d.outVelocity[2];
        state.downDash.available = false;
        state.downDash.didDownDash = true;
        events.didDownDash = true;
        return true;
    }

    state.baseVelocity.z += config.downDashVerticalSpeed;
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

    // Hot freeze policy: a hot handler owns activation/duration/suppression/exit.
    {
        GameFreezePolicyV1 fp{};
        fp.velocity[0] = state.baseVelocity.x;
        fp.velocity[1] = state.baseVelocity.y;
        fp.velocity[2] = state.baseVelocity.z;
        fp.dt = dt;
        fp.durationSeconds = config.freezeDurationSeconds;
        fp.freezePressed = command.freezePressed ? 1u : 0u;
        fp.freezeHeld = command.freezeHeld ? 1u : 0u;
        fp.freezeHeldPreviously = state.freeze.heldPreviously ? 1u : 0u;
        fp.freezeEnabled = config.freezeEnabled ? 1u : 0u;
        fp.freezeActive = state.freeze.active ? 1u : 0u;
        fp.freezeAvailable = state.freeze.available ? 1u : 0u;
        fp.freezeTimerSeconds = state.freeze.timerSeconds;
        fp.handled = 0;
        if (LiveBehavior::dispatchGameplayEvent64(GAME_EVENT_MOVEMENT_FREEZE, &fp,
                                                  sizeof(fp), 0, 0, 0) &&
            fp.handled) {
            state.baseVelocity.x = fp.outVelocity[0];
            state.baseVelocity.y = fp.outVelocity[1];
            state.baseVelocity.z = fp.outVelocity[2];
            state.freeze.active = fp.outFreezeActive != 0u;
            state.freeze.available = fp.outFreezeAvailable != 0u;
            state.freeze.heldPreviously = fp.outFreezeHeldPreviously != 0u;
            state.freeze.timerSeconds = fp.outFreezeTimerSeconds;
            if (fp.outDidFreeze != 0u) {
                state.externalImpulse = glm::vec3(0.0f);
                state.freeze.didFreeze = true;
                events.didFreeze = true;
                events.freezeStarted = true;
            }
            if (fp.outFreezeEnded != 0u)
                events.freezeEnded = true;
            return;
        }
    }

    if (!config.freezeEnabled) {
        // Toggle off mid-freeze: release the freeze so the player is not stuck.
        if (state.freeze.active) {
            state.freeze.active = false;
            events.freezeEnded = true;
        }
        state.freeze.heldPreviously = command.freezeHeld;
        return;
    }

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
        // Freeze stops everything, including external velocity.
        state.externalImpulse = glm::vec3(0.0f);
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
    // Hot gravity policy: a hot handler owns the vertical velocity change and
    // terminal clamp; fallback keeps the built-in math.
    GameGravityV1 g{};
    g.velocityZ = state.baseVelocity.z;
    g.gravityZ = config.gravityZ;
    g.maximumFallSpeed = config.maximumFallSpeed;
    g.dt = dt;
    g.handled = 0;
    if (LiveBehavior::dispatchGameplayEvent64(GAME_EVENT_MOVEMENT_GRAVITY, &g,
                                              sizeof(g), 0, 0, 0) &&
        g.handled) {
        state.baseVelocity.z = g.outVelocityZ;
    } else {
        state.baseVelocity.z = movementApplyGravityZ(
            state.baseVelocity.z, config.gravityZ, config.maximumFallSpeed, dt);
    }
}

void applyBasicExternalImpulseControl(MovementState& state,
                                      const MovementCommand& command)
{
    // Default (override) mode: movement input reclaims full control, killing
    // all external velocity (knockback, recoil, launches). Accel (CS) mode
    // never calls this and preserves external impulses through input.
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

// ── Source (CS/Quake PM_) movement model ──────────────────────────────
// Mirrors pm_shared.c: PM_Friction + PM_Accelerate (ground) and
// PM_AirAccelerate (air), with optional sv_aircontrol-style redirect and
// MiMITA-specific dash-grace + landing-bleed tuning knobs.

float sourceMaxSpeedValue(const MovementConfig& config, float sizeScale)
{
    // Hot speed policy: the one shared derivation of effective max speed.
    GameSpeedPolicyV1 p{};
    p.baseMaxSpeed = config.sourceMaxSpeed;
    p.baseFallbackSpeed = config.groundSpeed;
    p.sizeScale = sizeScale;
    p.sizeExponent = config.movementSpeedSizeExponent;
    p.speedLimit = config.speedLimit;
    p.airMaxWishspeed = 0.0f;
    p.rawWishSpeed = 0.0f;
    p.speedLimitFixed = 0u;
    p.handled = 0;
    if (LiveBehavior::dispatchGameplayEvent64(GAME_EVENT_MOVEMENT_SPEED_POLICY, &p,
                                              sizeof(p), 0, 0, 0) &&
        p.handled)
        return p.outMaxSpeed;

    const float base = config.sourceMaxSpeed > 0.0f
        ? config.sourceMaxSpeed
        : config.groundSpeed;
    return base * movementSizeScaleFactor(sizeScale, config.movementSpeedSizeExponent);
}

// Global speed limit: clamp horizontal velocity to the configured limit.
// Called at the end of each movement step regardless of movement mode.
void applySpeedLimitClamp(MovementState& state, const MovementConfig& config)
{
    if (!config.speedLimitEnabled || config.speedLimit <= 0.0f)
        return;

    // Hot post-step speed clamp / preservation policy.
    GameSpeedClampV1 c{};
    c.velocity[0] = state.baseVelocity.x;
    c.velocity[1] = state.baseVelocity.y;
    c.speedLimit = config.speedLimit;
    c.enabled = 1u;
    c.handled = 0;
    if (LiveBehavior::dispatchGameplayEvent64(GAME_EVENT_MOVEMENT_SPEED_CLAMP, &c,
                                              sizeof(c), 0, 0, 0) &&
        c.handled) {
        state.baseVelocity.x = c.outVelocity[0];
        state.baseVelocity.y = c.outVelocity[1];
        return;
    }

    glm::vec2 vel(state.baseVelocity.x, state.baseVelocity.y);
    const float speed = glm::length(vel);
    if (speed > config.speedLimit && speed > 0.0f)
        vel *= config.speedLimit / speed;
    state.baseVelocity.x = vel.x;
    state.baseVelocity.y = vel.y;
}

// Source-style external impulse: no exponential decay. The impulse carries
// through any input and is only bled by ground friction (stopspeed-based),
// plus a carry window where friction does not touch a fresh knockback.
void applySourceExternalImpulse(MovementState& state,
                                const MovementConfig& config,
                                float dt)
{
    glm::vec3& ext = state.externalImpulse;
    const float len = glm::length(ext);

    // A fresh impulse (magnitude grows) reopens the carry window.
    if (config.impulseCarrySeconds > 0.0f &&
        len > state.externalImpulseMagnitude + 0.01f)
        state.externalImpulseCarryTimerSeconds = config.impulseCarrySeconds;
    if (state.externalImpulseCarryTimerSeconds > 0.0f)
        state.externalImpulseCarryTimerSeconds =
            std::max(0.0f, state.externalImpulseCarryTimerSeconds - dt);
    state.externalImpulseMagnitude = len;

    if (len <= 0.0001f)
        return;

    if (state.ground.stableOnGround &&
        state.externalImpulseCarryTimerSeconds <= 0.0f) {
        glm::vec2 extXY(ext.x, ext.y);
        const float speed = glm::length(extXY);
        if (speed > 0.1f) {
            const float control = std::max(speed, config.stopspeed);
            const float drop = control * config.sourceFriction *
                               config.surfaceFriction * dt;
            const float newSpeed = std::max(0.0f, speed - drop);
            if (newSpeed != speed) {
                extXY *= newSpeed / speed;
                ext.x = extXY.x;
                ext.y = extXY.y;
            }
        } else {
            ext.x = 0.0f;
            ext.y = 0.0f;
        }
    }

    glm::vec2 extXY(ext.x, ext.y);
    const float impulseSpeed = glm::length(extXY);
    if (impulseSpeed > config.maximumExternalImpulseSpeed && impulseSpeed > 0.0f) {
        extXY *= config.maximumExternalImpulseSpeed / impulseSpeed;
        ext.x = extXY.x;
        ext.y = extXY.y;
    }
}

// Grounded Source step: friction every tick, accelerate along wishdir. No hard
// clamp to maxspeed (overspeed carries and bleeds via friction, like Source).
void applySourceGround(MovementState& state,
                       const MovementCommand& command,
                       const MovementConfig& config,
                       float dt)
{
    glm::vec2 vel(state.baseVelocity.x, state.baseVelocity.y);
    float maxSpeed = sourceMaxSpeedValue(config, state.sizeScale);
    if (config.speedLimitEnabled &&
        config.speedLimitMode == MovementSpeedLimitMode::Fixed &&
        config.speedLimit > 0.0f)
        maxSpeed = std::min(maxSpeed, config.speedLimit);
    const bool dashGrace = state.dash.dashGraceTimerSeconds > 0.0f;

    float frictionAmount = config.sourceFriction * config.surfaceFriction;
    const bool v206Walk = config.walkMode == MovementWalkMode::V206;
    if (v206Walk)
        frictionAmount = config.groundFrictionAmount;
    if (dashGrace)
        frictionAmount *= config.dashFrictionMultiplier;
    // Landing/autobhop tick: scale friction so bhop overspeed bleed is tunable
    // (1.0 = Source bleed, 0.0 = MiMITA preserve-through-landing).
    if (state.jump.jumpIntentTimerSeconds > 0.0f)
        frictionAmount *= config.landingOverspeedBleed;

    const glm::vec2 wish = movementClampUnitOrZero(command.moveAxes);
    const bool hasInput = movementHasMoveInput(wish);
    const glm::vec2 wishDir =
        hasInput ? movementNormalizeDirectionOrZero(wish) : glm::vec2(0.0f);

    // Hot ground-move algorithm: a hot handler owns friction + acceleration.
    // Fallback keeps the built-in Source math when no hot handler is active.
    {
        GameGroundMoveV1 g{};
        g.velocity[0] = vel.x;
        g.velocity[1] = vel.y;
        g.wishDir[0] = wishDir.x;
        g.wishDir[1] = wishDir.y;
        g.wishSpeed = maxSpeed;
        g.groundAcceleration = config.groundAcceleration;
        g.frictionAmount = frictionAmount;
        g.stopspeed = v206Walk ? 0.0f : config.stopspeed;
        g.dt = dt;
        g.hasInput = hasInput ? 1u : 0u;
        g.movementModel = v206Walk ? 1u : 0u;
        g.handled = 0;
        if (LiveBehavior::dispatchGameplayEvent64(GAME_EVENT_MOVEMENT_GROUND_MOVE, &g,
                                                  sizeof(g), 0, 0, 0) &&
            g.handled) {
            vel.x = g.outVelocity[0];
            vel.y = g.outVelocity[1];
        } else if (v206Walk) {
            // v2.0.6 fallback: friction XOR accelerate.
            if (hasInput) {
                const float currentSpeed = glm::dot(vel, wishDir);
                const float addSpeed = maxSpeed - currentSpeed;
                if (addSpeed > 0.0f) {
                    float accelSpeed = config.groundAcceleration * maxSpeed * dt;
                    accelSpeed = std::min(accelSpeed, addSpeed);
                    vel += wishDir * accelSpeed;
                }
            } else {
                const float speed = glm::length(vel);
                if (speed > 0.1f) {
                    const float drop = speed * frictionAmount * dt;
                    const float newSpeed = std::max(0.0f, speed - drop);
                    if (newSpeed != speed)
                        vel *= newSpeed / speed;
                } else {
                    vel = glm::vec2(0.0f);
                }
            }
        } else {
            // PM_Friction (horizontal only, every grounded tick).
            float speed = glm::length(vel);
            if (speed > 0.1f) {
                const float control = std::max(speed, config.stopspeed);
                const float drop = control * frictionAmount * dt;
                const float newSpeed = std::max(0.0f, speed - drop);
                if (newSpeed != speed)
                    vel *= newSpeed / speed;
            } else {
                vel = glm::vec2(0.0f);
            }

            // PM_Accelerate along wishdir (wishspeed capped to maxspeed).
            if (hasInput) {
                const float currentSpeed = glm::dot(vel, wishDir);
                const float addSpeed = maxSpeed - currentSpeed;
                if (addSpeed > 0.0f) {
                    float accelSpeed = config.groundAcceleration * maxSpeed * dt;
                    accelSpeed = std::min(accelSpeed, addSpeed);
                    vel += wishDir * accelSpeed;
                }
            }
        }
    }

    // No hard clamp to maxspeed: overspeed (bhop/dash/knockback) carries and is
    // bled by friction, exactly like Source. The accel term above only adds up
    // to maxspeed along the wish, so grounded walk speed still converges there.

    state.baseVelocity.x = vel.x;
    state.baseVelocity.y = vel.y;

    // Landing vertical snap.
    if (config.groundSnap &&
        std::fabs(state.baseVelocity.z) <= config.velocityClipEpsilon)
        state.baseVelocity.z = 0.0f;
}

// Airborne Source step: Source PM_AirAccelerate projection with optional
// directional blending. WASD defines wishdir (camera-relative, rebuilt by the
// input layer as the camera turns). The air_input_blending parameter controls
// how mouse movement transitions between directional accel (WASD pushes toward
// maxSpeed) and Source PM_AirAccelerate (wishspd projection cap). At 0 the
// transition is linear with mouse yaw speed; at 1 any mouse movement triggers
// full Source strafing.
//
//   currentSpeed = dot(vel, wishDir)
//   addSpeed     = wishspd - currentSpeed
//   if addSpeed <= 0: apply nothing
//   accelSpeed   = min(air_accel * wishspeed * dt * surfaceFriction, addSpeed)
//   vel += wishDir * accelSpeed
void applySourceAir(MovementState& state,
                    const MovementCommand& command,
                    const MovementConfig& config,
                    float dt)
{
    const glm::vec2 wish = movementClampUnitOrZero(command.moveAxes);
    const bool hasInput = movementHasMoveInput(wish);
    state.airDebug = MovementAirDebug{};
    state.airDebug.hasInput = hasInput;
    state.airDebug.grounded = state.ground.onGround;
    state.airDebug.branch = MovementAirDebug::Branch::Air;
    if (state.ground.onGround)
        return;
    state.airDebug.sourceBugCompatible =
        config.sourceAirAccelerateBugCompatible;
    glm::vec2 cameraForward = movementNormalizeDirectionOrZero(
        glm::vec2(command.horizontalCameraForward.x,
                  command.horizontalCameraForward.y));
    if (!movementHasMoveInput(cameraForward))
        cameraForward = movementHorizontalForwardFromYaw(command.lookYaw);
    const glm::vec2 cameraRight(cameraForward.y, -cameraForward.x);
    state.airDebug.forwardMove = glm::dot(wish, cameraForward);
    state.airDebug.sideMove = glm::dot(wish, cameraRight);
    glm::vec2 vel(state.baseVelocity.x, state.baseVelocity.y);
    state.airDebug.horizontalVelocity = vel;
    state.airDebug.horizontalSpeed = glm::length(vel);
    state.airDebug.finalHorizontalSpeed = state.airDebug.horizontalSpeed;
    if (!hasInput)
        return;

    const float maxSpeed = config.speedLimitEnabled &&
        config.speedLimitMode == MovementSpeedLimitMode::Fixed &&
        config.speedLimit > 0.0f
        ? std::min(sourceMaxSpeedValue(config, state.sizeScale), config.speedLimit)
        : sourceMaxSpeedValue(config, state.sizeScale);
    const glm::vec2 wishVelocity = wish * maxSpeed;
    const float wishSpeed = glm::length(wishVelocity);
    const glm::vec2 wishDir = movementNormalizeDirectionOrZero(wishVelocity);
    state.airDebug.wishVelocity = wishVelocity;
    state.airDebug.wishSpeed = wishSpeed;
    state.airDebug.wishDir = wishDir;

    // v2.0.6 air model: additive impulse toward the base move speed, no
    // projection-cap curve, no from-rest restriction.
    if (config.walkMode == MovementWalkMode::V206) {
        GameAirAccelerateV1 policy{};
        policy.velocity[0] = vel.x;
        policy.velocity[1] = vel.y;
        policy.wishDir[0] = wishDir.x;
        policy.wishDir[1] = wishDir.y;
        policy.wishSpeed = maxSpeed;
        policy.airAcceleration = config.airAcceleration;
        policy.surfaceFriction = 1.0f;
        policy.airSpeedGainMultiplier = 0.0f;
        policy.dt = dt;
        policy.movementModel = 1u;
        policy.handled = 0;
        if (LiveBehavior::dispatchGameplayEvent64(GAME_EVENT_MOVEMENT_AIR_ACCELERATE,
                                                  &policy, sizeof(policy), 0, 0, 0) &&
            policy.handled) {
            vel.x = policy.outVelocity[0];
            vel.y = policy.outVelocity[1];
        } else {
            const float projected = vel.x * wishDir.x + vel.y * wishDir.y;
            const float addSpeed = maxSpeed - projected;
            if (addSpeed > 0.0f) {
                float accelSpeed = config.airAcceleration * maxSpeed * dt;
                accelSpeed = std::min(accelSpeed, addSpeed);
                vel += wishDir * accelSpeed;
            }
        }
        state.baseVelocity.x = vel.x;
        state.baseVelocity.y = vel.y;
        applySpeedLimitClamp(state, config);
        return;
    }

    // Source caps wishspd for projection. The original code still uses the
    // uncapped wishspeed in accelspeed; expose that historical quirk explicitly.
    // Hot speed policy: the air wish-speed projection cap is derived by the same
    // shared function the client uses.
    float wishspd = maxSpeed;
    {
        GameSpeedPolicyV1 sp{};
        sp.baseMaxSpeed = config.sourceMaxSpeed;
        sp.baseFallbackSpeed = config.groundSpeed;
        sp.sizeScale = state.sizeScale;
        sp.sizeExponent = config.movementSpeedSizeExponent;
        sp.speedLimit = config.speedLimit;
        sp.airMaxWishspeed = config.airMaxWishspeed;
        sp.rawWishSpeed = wishSpeed;
        sp.speedLimitFixed = 0u;
        sp.handled = 0;
        if (LiveBehavior::dispatchGameplayEvent64(GAME_EVENT_MOVEMENT_SPEED_POLICY,
                                                  &sp, sizeof(sp), 0, 0, 0) &&
            sp.handled) {
            wishspd = sp.outWishspd;
        } else {
            wishspd = config.airMaxWishspeed > 0.0f
                ? config.airMaxWishspeed *
                      movementSizeScaleFactor(state.sizeScale,
                                              config.movementSpeedSizeExponent)
                : maxSpeed;
            wishspd = std::min(wishspd, wishSpeed);
        }
    }
    state.airDebug.cappedWishSpeed = wishspd;

    state.airDebug.currentSpeed = glm::dot(vel, wishDir);
    if (state.airDebug.horizontalSpeed <= 0.1f)
        return; // MiMITA Source contract: air input cannot launch from rest.

    float blendedAddSpeed;
    if (config.airInputBlendingEnabled) {
        // Air input blending: directional accel (push toward maxSpeed) vs Source
        // PM_AirAccelerate (wishspd projection cap). The mouse yaw delta drives
        // a 0..1 mouseFactor; the blend parameter curves the transition.
        const float yawDelta = std::abs(
            shortestSignedAngleDegrees(state.previousYaw, command.lookYaw));
        const float threshold = std::max(0.1f, config.airInputMouseThresholdDegrees);
        const float mouseFactor = std::clamp(yawDelta / threshold, 0.0f, 1.0f);
        const bool mouseMoved = yawDelta > 0.01f;
        const float sourceWeight = glm::mix(
            mouseFactor,
            mouseMoved ? 1.0f : 0.0f,
            config.airInputBlending);
        state.airDebug.mouseFactor = mouseFactor;
        state.airDebug.sourceWeight = sourceWeight;

        const float directionalAddSpeed = maxSpeed - state.airDebug.currentSpeed;
        const float sourceAddSpeed = wishspd - state.airDebug.currentSpeed;
        blendedAddSpeed = glm::mix(directionalAddSpeed, sourceAddSpeed, sourceWeight);
    } else {
        // Pure Source: wishspd projection cap.
        blendedAddSpeed = wishspd - state.airDebug.currentSpeed;
    }
    state.airDebug.addSpeed = blendedAddSpeed;

    // Hot movement algorithm: a hot handler owns the air-acceleration math. If
    // it handles this step, its velocity replaces the built-in formula. Inputs
    // are plain numbers, so the same hot function serves server + prediction.
    {
        GameAirAccelerateV1 policy{};
        policy.velocity[0] = vel.x;
        policy.velocity[1] = vel.y;
        policy.wishDir[0] = wishDir.x;
        policy.wishDir[1] = wishDir.y;
        policy.wishSpeed = wishSpeed;
        policy.wishspd = wishspd;
        policy.maxSpeed = maxSpeed;
        policy.airAcceleration = config.airAcceleration;
        policy.surfaceFriction = config.surfaceFriction;
        policy.airSpeedGainMultiplier = config.airSpeedGainMultiplier;
        policy.dt = dt;
        policy.currentSpeed = state.airDebug.currentSpeed;
        policy.blendedAddSpeed = blendedAddSpeed;
        policy.handled = 0;
        if (LiveBehavior::dispatchGameplayEvent64(
                GAME_EVENT_MOVEMENT_AIR_ACCELERATE, &policy, sizeof(policy), 0, 0, 0) &&
            policy.handled) {
            vel.x = policy.outVelocity[0];
            vel.y = policy.outVelocity[1];
            state.baseVelocity.x = vel.x;
            state.baseVelocity.y = vel.y;
            state.airDebug.applied = true;
            state.airDebug.finalHorizontalSpeed = glm::length(vel);
            return;
        }
    }

    if (blendedAddSpeed > 0.0f) {
        const float accelerationWishSpeed =
            config.sourceAirAccelerateBugCompatible ? wishSpeed : wishspd;
        const float baseAccelSpeed = config.airAcceleration * accelerationWishSpeed *
                                     dt * config.surfaceFriction *
                                     config.airSpeedGainMultiplier;

        // Directional uses uncapped wishSpeed in the accel term; Source uses
        // the bug-compatible uncapped value too, so the accel term is the same
        // for both. Only the addSpeed cap differs (maxSpeed vs wishspd).
        const float accelSpeed = std::min(baseAccelSpeed, blendedAddSpeed);
        if (accelSpeed > 0.0f) {
            state.airDebug.accelSpeed = accelSpeed;
            state.airDebug.applied = true;
            vel += wishDir * accelSpeed;
        }
    }

    state.baseVelocity.x = vel.x;
    state.baseVelocity.y = vel.y;
    state.airDebug.finalHorizontalSpeed = glm::length(vel);
}

// Dispatcher: runs the Source step every tick (friction even with no input).
void applySourceMovement(MovementState& state,
                         const MovementCommand& command,
                         const MovementConfig& config,
                         float dt)
{
    state.airDebug = MovementAirDebug{};
    state.airDebug.grounded = state.ground.onGround;
    state.airDebug.sourceBugCompatible =
        config.sourceAirAccelerateBugCompatible;
    if (state.dash.dashGraceTimerSeconds > 0.0f)
        state.dash.dashGraceTimerSeconds =
            std::max(0.0f, state.dash.dashGraceTimerSeconds - dt);

    const bool jumpingNow = state.ground.onGround &&
        (command.jumpPressed || (config.autoBhopEnabled && command.jumpHeld));
    if (state.ground.onGround && !jumpingNow) {
        state.airDebug.branch = MovementAirDebug::Branch::Ground;
        state.airDebug.hasInput = movementHasMoveInput(command.moveAxes);
        state.airDebug.horizontalVelocity =
            glm::vec2(state.baseVelocity.x, state.baseVelocity.y);
        state.airDebug.horizontalSpeed =
            glm::length(state.airDebug.horizontalVelocity);
        state.airDebug.finalHorizontalSpeed = state.airDebug.horizontalSpeed;
        applySourceGround(state, command, config, dt);
    } else if (config.airControlEnabled) {
        state.airDebug.branch = MovementAirDebug::Branch::Air;
        applySourceAir(state, command, config, dt);
    }

    applySpeedLimitClamp(state, config);
}

void applyBasicWalk(MovementState& state,
                    const MovementCommand& command,
                    const MovementConfig& config,
                    float fixedDt)
{
    if (config.walkMode == MovementWalkMode::Source ||
        config.walkMode == MovementWalkMode::V206) {
        const float dt = std::max(movementClampStepDelta(fixedDt, config), 0.0001f);
        applySourceMovement(state, command, config, dt);
        return;
    }

    // Air strafing toggle: while airborne, WASD does not steer when disabled.
    if (!config.airControlEnabled && !state.ground.onGround)
        return;

    const glm::vec2 wish = movementClampUnitOrZero(command.moveAxes);

    // Override mode: instant velocity assignment (legacy behavior, unchanged).
    if (config.walkMode != MovementWalkMode::Accel) {
        if (!movementHasMoveInput(wish))
            return;
        const glm::vec2 next =
            movementWalkVelocityXY(glm::vec2(state.baseVelocity),
                                   wish,
                                   state.ground.onGround,
                                   state.sizeScale,
                                   config);
        state.baseVelocity.x = next.x;
        state.baseVelocity.y = next.y;
        applySpeedLimitClamp(state, config);
        return;
    }

    const float dt = std::max(movementClampStepDelta(fixedDt, config), 0.0001f);

    // Grounded: converge toward the desired camera-relative velocity.
    // Air acceleration must never run while grounded.
    if (state.ground.onGround) {
        applyGroundControl(state, wish, config, dt);
        return;
    }

    // Airborne: steering (speed-preserving) + speed gain (valid strafe only).
    if (!movementHasMoveInput(wish))
        return;
    const glm::vec2 wishDir = movementNormalizeDirectionOrZero(wish);
    const AirStrafeEvaluation ev = evaluateAirStrafe(state, command, config);
    applyAirSteering(state, wishDir, config, dt, ev);
    applyAirSpeedGain(state, wishDir, config, dt, ev, glm::length(wish));
    applySpeedLimitClamp(state, config);
}

void applyBasicJump(MovementState& state,
                    const MovementCommand& command,
                    const MovementConfig& config,
                    float fixedDt,
                    MovementStepEvents* events)
{
    const float dt = movementClampStepDelta(fixedDt, config);

    // Hot jump policy: a hot handler owns the whole jump decision + state.
    {
        GameJumpPolicyV1 j{};
        j.velocityZ = state.baseVelocity.z;
        j.jumpSpeed = movementScaledJumpVelocity(config, state.sizeScale);
        j.dt = dt;
        j.coyoteSeconds = config.coyoteSeconds;
        j.jumpBufferSeconds = config.jumpBufferSeconds;
        j.grounded = state.ground.onGround ? 1u : 0u;
        j.jumpPressed = command.jumpPressed ? 1u : 0u;
        j.jumpHeld = command.jumpHeld ? 1u : 0u;
        j.jumpHeldPreviously = state.jump.jumpHeldPreviously ? 1u : 0u;
        j.autoBhopEnabled = config.autoBhopEnabled ? 1u : 0u;
        j.maximumAirJumps = static_cast<std::uint32_t>(
            config.maximumAirJumps > 0 ? config.maximumAirJumps : 0);
        j.jumpIntentTimerSeconds = state.jump.jumpIntentTimerSeconds;
        j.coyoteTimerSeconds = state.jump.coyoteTimerSeconds;
        j.airJumpsLeft = state.jump.airJumpsLeft;
        j.airJumpArmed = state.jump.airJumpArmed ? 1u : 0u;
        j.airJumpLocked = state.jump.airJumpLocked ? 1u : 0u;
        j.handled = 0;
        if (LiveBehavior::dispatchGameplayEvent64(GAME_EVENT_MOVEMENT_JUMP, &j,
                                                  sizeof(j), 0, 0, 0) &&
            j.handled) {
            state.baseVelocity.z = j.outVelocityZ;
            state.jump.jumpIntentTimerSeconds = j.jumpIntentTimerSeconds;
            state.jump.coyoteTimerSeconds = j.coyoteTimerSeconds;
            state.jump.airJumpsLeft = j.airJumpsLeft;
            state.jump.airJumpArmed = j.airJumpArmed != 0u;
            state.jump.airJumpLocked = j.airJumpLocked != 0u;
            state.ground.onGround = j.outGrounded != 0u;
            state.jump.jumpHeldPreviously = j.outJumpHeldPreviously != 0u;
            state.jump.didGroundJump = j.outDidGroundJump != 0u;
            state.jump.didAirJump = j.outDidAirJump != 0u;
            if (j.outDidGroundJump != 0u)
                state.dash.dashAvailable = true;
            if (events) {
                if (j.outDidGroundJump != 0u)
                    events->didGroundJump = true;
                if (j.outDidAirJump != 0u)
                    events->didAirJump = true;
            }
            return;
        }
    }

    state.jump.jumpIntentTimerSeconds =
        std::max(0.0f, state.jump.jumpIntentTimerSeconds - dt);
    state.jump.coyoteTimerSeconds =
        std::max(0.0f, state.jump.coyoteTimerSeconds - dt);

    if (state.ground.onGround)
        state.jump.coyoteTimerSeconds = config.coyoteSeconds;

    if (command.jumpHeld && config.autoBhopEnabled)
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

    if (config.walkMode == MovementWalkMode::Source) {
        // Ground friction is owned by the Source ground step; here we only
        // shape the external impulse.
        const float frictionOverride =
            std::clamp(state.dash.frictionOverride, 0.0f, 1.0f);
        if (config.impulseFrictionMode == MovementImpulseFrictionMode::Source)
            applySourceExternalImpulse(state, config, dt);
        else
            movementDecayAndClampExternalImpulse(
                state.externalImpulse, config, frictionOverride, dt);
        return;
    }

    const bool hasMoveInput = movementHasMoveInput(state.lastInputMoveAxes);
    const float frictionOverride = std::clamp(state.dash.frictionOverride, 0.0f, 1.0f);

    if (config.walkMode != MovementWalkMode::Accel) {
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
    } else if (!state.ground.onGround && config.airFrictionAmount > 0.0f) {
        // Airborne: optional air drag. Ground stopping is owned by the ground
        // control controller, so no player-controlled ground friction here.
        glm::vec2 vel(state.baseVelocity.x, state.baseVelocity.y);
        vel *= expDecay(config.airFrictionAmount * frictionOverride, dt);
        state.baseVelocity.x = vel.x;
        state.baseVelocity.y = vel.y;
    }

    movementDecayAndClampExternalImpulse(
        state.externalImpulse, config, frictionOverride, dt);
}

void applyPreCollisionBasicMovement(MovementState& state,
                                    const MovementCommand& command,
                                    const MovementConfig& config,
                                    float fixedDt)
{
    // 08 16 2026 — KNOCKBACK CONSUMPTION: COMBINED SINGLE-TICK IMPULSE
    // WAS: external impulse lived in a separate tank, was re-added to the move
    // every tick, and decayed/bled off over many ticks (lingering "sticky" push).
    // NOW: it is combined into the real velocity ONCE per tick, then cleared, so
    // gravity, friction, air control, and input own the momentum from the next
    // tick on (Source-style momentum you can bhop-carry).
    // TO REVERT: delete this block and external impulse goes back to the old
    // separate-tank decay behavior (impulse_friction_mode / external_impulse_decay
    // / impulse_carry_seconds in the movement preset), restoring the old tests.
    const glm::vec3 pendingImpulse = state.externalImpulse;
    if (glm::length(pendingImpulse) > 0.0001f)
    {
        glm::vec2 impulseXY(pendingImpulse.x, pendingImpulse.y);
        if (config.maximumExternalImpulseSpeed > 0.0f)
        {
            const float impulseSpeed = glm::length(impulseXY);
            if (impulseSpeed > config.maximumExternalImpulseSpeed && impulseSpeed > 0.0f)
                impulseXY *= config.maximumExternalImpulseSpeed / impulseSpeed;
        }
        state.baseVelocity.x += impulseXY.x;
        state.baseVelocity.y += impulseXY.y;
        state.baseVelocity.z += pendingImpulse.z;
    }
    state.externalImpulse = glm::vec3(0.0f);
    state.externalImpulseCarryTimerSeconds = 0.0f;
    state.externalImpulseMagnitude = 0.0f;

    state.previousMoveAxes = state.lastInputMoveAxes;
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
    MovementStepEvents& events)
{
    const float dt = movementClampStepDelta(fixedDt, config);
    const bool previousOnGround = state.ground.onGround;
    const bool previousStableOnGround = state.ground.stableOnGround;
    const float previousAirborneSeconds = state.ground.airborneTimerSeconds;

    // Track the previous camera yaw so the airborne strafe-eligibility layer
    // can measure per-tick camera rotation deterministically on client+server.
    state.previousYaw = state.yaw;
    state.yaw = command.lookYaw;

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

    // Default (override) mode: WASD/dash input kills external velocity so the
    // player regains full control. Accel (CS) mode preserves external impulses
    // through input (its movement controllers own the player velocity).
    // Source mode also preserves impulses and runs every tick.
    const bool sourceMode = config.walkMode == MovementWalkMode::Source;
    if (config.walkMode == MovementWalkMode::Override) {
        if (specialMovementEnabled) {
            updateDashMomentumProtectionForWalk(state, command);
            applySpecialExternalImpulseControl(state, command);
        } else {
            applyBasicExternalImpulseControl(state, command);
        }
    } else if (specialMovementEnabled && !sourceMode) {
        updateDashMomentumProtectionForWalk(state, command);
    }

    if (sourceMode) {
        // Source step runs every tick (friction always applies on the ground,
        // input never overwrites velocity).
        applyBasicWalk(state, command, config, dt);
    } else {
        const bool walkingMayOverwriteDash =
            !specialMovementEnabled ||
            shouldWalkingOverwriteDashMomentum(state, command);
        if (command.movementDirectionPressed &&
            state.dash.frictionOverride >= 1.0f &&
            walkingMayOverwriteDash) {
            applyBasicWalk(state, command, config, dt);
        }
    }

    if (specialMovementEnabled)
        applySpecialMovementPostCollision(state, command, config, dt, events);
    applyBasicJump(state, command, config, dt, &events);
    if (!sourceMode)
        applyBasicFrictionOverrideRecovery(state, command);
    applyBasicFriction(state, config, dt);
    applyBasicLandingTimers(
        state, config, previousStableOnGround, previousAirborneSeconds,
        collision.impactSpeed, dt, events);

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
    MovementStepEvents events;
    return applyPostCollisionMovementInternal(
        state, command, config, collision, fixedDt, false, events);
}

MovementStepResult applyPostCollisionMovementWithSpecials(
    MovementState& state,
    const MovementCommand& command,
    const MovementConfig& config,
    const MovementCollisionFeedback& collision,
    float fixedDt,
    MovementStepEvents& preCollisionEvents)
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
