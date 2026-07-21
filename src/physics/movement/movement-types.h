// 07 21 2026, 15 45
/* purpose
* Defines neutral movement command, state, config, contact, and event data.
* Provides deterministic helpers needed by shared client/server movement.
* Keeps movement vocabulary independent from Player, server, rendering, audio, and transport.
* Does NOT send packets, decide network authority, or own collision sweeps.
* Does NOT apply damage, play effects, or mutate runtime Player state.
* Does NOT include presentation-only animation, audio, or network serial fields.
*/

#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

constexpr float MOVEMENT_INPUT_EPSILON = 0.0001f;

struct MovementLifecycleIdentity {
    uint32_t spawnGeneration = 0;
    uint32_t transformEpoch = 0;
};

inline bool sameMovementLifecycle(const MovementLifecycleIdentity& a,
                                  const MovementLifecycleIdentity& b)
{
    return a.spawnGeneration == b.spawnGeneration &&
           a.transformEpoch == b.transformEpoch;
}

struct MovementDirectionTransition {
    bool previousPressed = false;
    bool currentPressed = false;
    bool freshPress = false;
    bool released = false;
    bool directionChanged = false;
    bool meaningfulChange = false;
};

inline glm::vec2 movementClampUnitOrZero(glm::vec2 axes,
                                         float epsilon = MOVEMENT_INPUT_EPSILON)
{
    const float lenSq = axes.x * axes.x + axes.y * axes.y;
    if (lenSq <= epsilon * epsilon)
        return glm::vec2(0.0f);
    if (lenSq > 1.0f)
        return axes / std::sqrt(lenSq);
    return axes;
}

inline glm::vec2 movementNormalizeDirectionOrZero(glm::vec2 axes,
                                                  float epsilon = MOVEMENT_INPUT_EPSILON)
{
    const float lenSq = axes.x * axes.x + axes.y * axes.y;
    if (lenSq <= epsilon * epsilon)
        return glm::vec2(0.0f);
    return axes / std::sqrt(lenSq);
}

inline MovementDirectionTransition movementDirectionTransition(glm::vec2 previousAxes,
                                                               glm::vec2 currentAxes,
                                                               float epsilon = MOVEMENT_INPUT_EPSILON)
{
    MovementDirectionTransition transition;
    const glm::vec2 previous = movementNormalizeDirectionOrZero(previousAxes, epsilon);
    const glm::vec2 current = movementNormalizeDirectionOrZero(currentAxes, epsilon);
    transition.previousPressed = previous.x != 0.0f || previous.y != 0.0f;
    transition.currentPressed = current.x != 0.0f || current.y != 0.0f;
    transition.freshPress = !transition.previousPressed && transition.currentPressed;
    transition.released = transition.previousPressed && !transition.currentPressed;
    if (transition.previousPressed && transition.currentPressed) {
        const glm::vec2 delta = current - previous;
        transition.directionChanged = (delta.x * delta.x + delta.y * delta.y) > epsilon * epsilon;
    }
    transition.meaningfulChange = transition.freshPress || transition.directionChanged;
    return transition;
}

struct MovementCommand {
    uint32_t sequence = 0;
    uint64_t clientSimulationTick = 0;
    MovementLifecycleIdentity lifecycle;

    glm::vec2 moveAxes{0.0f};
    glm::vec3 horizontalCameraForward{1.0f, 0.0f, 0.0f};
    float lookYaw = 0.0f;
    float lookPitch = 0.0f;

    bool jumpHeld = false;
    bool jumpPressed = false;
    bool dashPressed = false;
    bool downDashPressed = false;
    bool groundReturnPressed = false;
    bool freezeHeld = false;
    bool freezePressed = false;

    bool movementDirectionPressed = false;
    bool movementDirectionFreshPressed = false;
    bool movementDirectionReleased = false;
    bool movementDirectionChanged = false;
    float movementHeldDurationSeconds = 0.0f;
};

struct MovementGroundState {
    bool onGround = false;
    bool stableOnGround = false;
    bool wasOnGround = false;
    bool hasWorldContact = false;
    bool realWorldContactThisFrame = false;
    bool didLand = false;
    glm::vec3 groundNormal{0.0f, 0.0f, 1.0f};
    float groundLostTimerSeconds = 0.0f;
    float airborneTimerSeconds = 0.0f;
    float landingCooldownSeconds = 0.0f;
    float worldContactLostTimerSeconds = 0.0f;
};

struct MovementJumpState {
    int airJumpsLeft = 0;
    bool jumpHeldPreviously = false;
    bool airJumpLocked = false;
    bool airJumpArmed = false;
    float jumpIntentTimerSeconds = 0.0f;
    float coyoteTimerSeconds = 0.0f;
    bool didGroundJump = false;
    bool didAirJump = false;
};

struct MovementDashState {
    bool dashAvailable = true;
    bool dashHeldPreviously = false;
    bool moveHeldPreviously = false;
    int dashMovementTicks = 0;
    int lastDashQuality = 0;
    bool didDash = false;
    float frictionOverride = 1.0f;
    bool tickPerfectDash = false;
};

struct MovementDownDashState {
    bool available = true;
    bool didDownDash = false;
};

struct MovementFreezeState {
    bool available = true;
    bool heldPreviously = false;
    bool active = false;
    float timerSeconds = 0.0f;
    bool didFreeze = false;
};

struct MovementGroundReturnState {
    bool available = true;
    int charges = 0;
    float rechargeTimerSeconds = 0.0f;
};

struct MovementDashMomentumProtectionState {
    bool active = false;
    glm::vec2 protectedMoveAxes{0.0f};
    bool usedCameraForwardFallback = false;
    uint32_t movementInputGeneration = 0;
};

struct MovementState {
    MovementLifecycleIdentity lifecycle;
    bool movementEnabled = true;

    glm::vec3 position{0.0f};
    glm::vec3 baseVelocity{0.0f};
    glm::vec3 externalImpulse{0.0f};
    glm::vec2 lastInputMoveAxes{0.0f};
    float yaw = 0.0f;
    float sizeScale = 1.0f;

    MovementGroundState ground;
    MovementJumpState jump;
    MovementDashState dash;
    MovementDownDashState downDash;
    MovementFreezeState freeze;
    MovementGroundReturnState groundReturn;
    MovementDashMomentumProtectionState dashMomentumProtection;
};

struct MovementConfig {
    uint32_t simulationHz = 60;
    float fixedDeltaSeconds = 1.0f / 60.0f;
    float maximumDeltaSeconds = 0.033f;

    float groundSpeed = 0.0f;
    float airSpeed = 0.0f;
    float movementSpeedSizeExponent = 0.5f;
    float gravityZ = 0.0f;
    float maximumFallSpeed = 0.0f;

    float jumpVerticalSpeed = 0.0f;
    float jumpHeightSizeExponent = 0.5f;
    float jumpBufferSeconds = 0.0f;
    float coyoteSeconds = 0.0f;
    int maximumAirJumps = 0;

    float groundDashImpulse = 0.0f;
    float airDashImpulse = 0.0f;
    float dashHorizontalImpulse = 0.0f;
    float downDashVerticalSpeed = 0.0f;
    float groundReturnVerticalSpeed = 0.0f;

    float freezeDurationSeconds = 0.0f;
    float freezeCurveExponent = 4.0f;
    float freezeDashMinimumPassThrough = 0.001f;

    float maximumExternalImpulseSpeed = 0.0f;
    float externalImpulseDecay = 0.0f;
    float externalImpulseSteerRate = 0.0f;
    float externalImpulseBrakeRate = 0.0f;
    float groundFrictionAmount = 0.0f;
    float airFrictionAmount = 0.0f;
    float frictionSizeExponent = -0.5f;
    float almostZeroSpeed = 0.00001f;

    float walkableSlopeDot = 0.0f;
    float collisionSkin = 0.0f;
    float maximumStepHeight = 0.0f;
    float stableGroundGraceSeconds = 0.08f;
    float landingMinimumAirborneSeconds = 0.08f;
    float landingCooldownResetSeconds = 0.3f;
};

enum class MovementContactKind : uint8_t {
    Unknown,
    Ground,
    Wall,
    Ceiling,
    Slope,
    Step,
    StaticWorld,
    MovingWorld,
    PlayerRoot,
    PlayerBody,
    Weapon,
    Projectile,
    Explosion,
    Water,
    Ladder,
    OtherGameplay
};

struct MovementContact {
    MovementContactKind kind = MovementContactKind::Unknown;
    MovementLifecycleIdentity targetLifecycle;
    uint32_t sourceEntityId = 0;
    uint32_t eventId = 0;
    glm::vec3 point{0.0f};
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
    glm::vec3 surfaceVelocity{0.0f};
    float penetrationDepth = 0.0f;
};

enum class MovementExternalEventType : uint8_t {
    AddImpulse,
    SetVelocity,
    Teleport,
    ContactReset,
    AttachToMovingSurface,
    DetachFromMovingSurface
};

struct MovementExternalEvent {
    MovementExternalEventType type = MovementExternalEventType::AddImpulse;
    MovementLifecycleIdentity targetLifecycle;
    uint32_t eventId = 0;
    uint32_t sourceEntityId = 0;
    glm::vec3 vector{0.0f};
    glm::vec3 point{0.0f};
    float scalar = 0.0f;
    float durationSeconds = 0.0f;
    bool authoritative = false;
};

inline bool movementEventMatchesLifecycle(const MovementExternalEvent& event,
                                          const MovementLifecycleIdentity& lifecycle)
{
    return sameMovementLifecycle(event.targetLifecycle, lifecycle);
}

struct MovementStepEvents {
    bool didGroundJump = false;
    bool didAirJump = false;
    bool leftGround = false;
    bool didDash = false;
    bool didDownDash = false;
    bool didLand = false;
    bool didFreeze = false;
    bool freezeStarted = false;
    bool freezeEnded = false;
    bool dashRejectedByFreeze = false;
    bool touchedGround = false;
    bool touchedWall = false;
    bool touchedCeiling = false;
    bool resetAirControlAbilities = false;
    std::vector<MovementContact> contacts;
    std::vector<MovementExternalEvent> consumedExternalEvents;
};

struct MovementStepResult {
    MovementState state;
    MovementStepEvents events;
};

struct MovementVelocityView {
    glm::vec3 effectiveBaseVelocity{0.0f};
    glm::vec3 effectiveExternalImpulse{0.0f};
    float horizontalPassThrough = 1.0f;
};

inline float movementFreezeHorizontalPassThrough(float elapsedSeconds,
                                                 float durationSeconds = 5.0f,
                                                 float exponent = 4.0f)
{
    if (elapsedSeconds <= 0.0f)
        return 0.0f;
    if (durationSeconds <= 0.0f)
        return 1.0f;

    float t = elapsedSeconds / durationSeconds;
    if (t <= 0.0f)
        return 0.0f;
    if (t >= 1.0f)
        return 1.0f;
    return std::pow(t, exponent);
}

inline bool movementIsFinite(float value)
{
    return std::isfinite(value);
}

inline bool movementIsFinite(glm::vec2 value)
{
    return movementIsFinite(value.x) && movementIsFinite(value.y);
}

inline bool movementIsFinite(glm::vec3 value)
{
    return movementIsFinite(value.x) && movementIsFinite(value.y) &&
           movementIsFinite(value.z);
}

inline bool movementIsFinite(const MovementCommand& command)
{
    return movementIsFinite(command.moveAxes) &&
           movementIsFinite(command.horizontalCameraForward) &&
           movementIsFinite(command.lookYaw) &&
           movementIsFinite(command.lookPitch) &&
           movementIsFinite(command.movementHeldDurationSeconds);
}

inline bool movementIsFinite(const MovementState& state)
{
    return movementIsFinite(state.position) &&
           movementIsFinite(state.baseVelocity) &&
           movementIsFinite(state.externalImpulse) &&
           movementIsFinite(state.lastInputMoveAxes) &&
           movementIsFinite(state.yaw) &&
           movementIsFinite(state.sizeScale) &&
           movementIsFinite(state.ground.groundNormal) &&
           movementIsFinite(state.ground.groundLostTimerSeconds) &&
           movementIsFinite(state.ground.airborneTimerSeconds) &&
           movementIsFinite(state.ground.landingCooldownSeconds) &&
           movementIsFinite(state.ground.worldContactLostTimerSeconds) &&
           movementIsFinite(state.jump.jumpIntentTimerSeconds) &&
           movementIsFinite(state.jump.coyoteTimerSeconds) &&
           movementIsFinite(state.dash.frictionOverride) &&
           movementIsFinite(state.freeze.timerSeconds) &&
           movementIsFinite(state.groundReturn.rechargeTimerSeconds) &&
           movementIsFinite(state.dashMomentumProtection.protectedMoveAxes);
}
