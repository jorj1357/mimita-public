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

#include <array>
#include <cmath>
#include <cstddef>
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
    // Source mode: while > 0 the dash boost is protected from the ground
    // controller's maxspeed clamp and from friction (scaled by dashFrictionMultiplier).
    float dashGraceTimerSeconds = 0.0f;
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

// Per-tick air-strafe projection values for the debug overlay / console.
// Filled by applySourceAir in Source mode so the player can SEE why speed was
// gained or not, instead of guessing.
struct MovementAirDebug {
    bool hasInput = false;
    bool applied = false;
    float horizontalSpeed = 0.0f;
    float currentSpeed = 0.0f;   // dot(velocity, wishDir)
    float addSpeed = 0.0f;
    float accelSpeed = 0.0f;
    glm::vec2 wishDir{0.0f};
    glm::vec2 horizontalVelocity{0.0f};
};

struct MovementDashMomentumProtectionState {
    bool active = false;
    glm::vec2 protectedMoveAxes{0.0f};
    bool usedCameraForwardFallback = false;
    uint32_t movementInputGeneration = 0;
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

enum class MovementContactSource : uint8_t {
    Unknown,
    StaticWorld,
    MovingWorld,
    MovingStructure,
    PlayerRoot,
    PlayerBody,
    Weapon,
    FriendlyWeapon,
    EnemyWeapon,
    OwnWeapon,
    Projectile,
    FriendlyProjectile,
    EnemyProjectile,
    OwnProjectile,
    RollingGrenade,
    Explosion,
    Water,
    Ladder,
    OtherGameplay
};

struct MovementContact {
    MovementContactKind kind = MovementContactKind::Unknown;
    MovementContactSource source = MovementContactSource::Unknown;
    MovementLifecycleIdentity targetLifecycle;
    uint64_t contactId = 0;
    uint64_t sourceEventId = 0;
    uint32_t sourceEntityId = 0;
    uint32_t surfaceId = 0;
    uint64_t simulationTick = 0;
    glm::vec3 point{0.0f};
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
    glm::vec3 surfaceVelocity{0.0f};
    float strength = 0.0f;
    float penetrationDepth = 0.0f;
    bool resetsAbilities = true;
};

inline int movementQuantizeContactComponent(float value)
{
    if (!std::isfinite(value))
        return 0;
    return static_cast<int>(std::round(value * 1024.0f));
}

inline bool movementContactMatchesLifecycle(const MovementContact& contact,
                                            const MovementLifecycleIdentity& lifecycle)
{
    return sameMovementLifecycle(contact.targetLifecycle, lifecycle);
}

inline bool movementContactsMatchTickLocalIdentity(const MovementContact& a,
                                                   const MovementContact& b)
{
    return a.simulationTick == b.simulationTick &&
           a.kind == b.kind &&
           a.source == b.source &&
           a.sourceEntityId == b.sourceEntityId &&
           a.surfaceId == b.surfaceId &&
           sameMovementLifecycle(a.targetLifecycle, b.targetLifecycle) &&
           movementQuantizeContactComponent(a.normal.x) ==
               movementQuantizeContactComponent(b.normal.x) &&
           movementQuantizeContactComponent(a.normal.y) ==
               movementQuantizeContactComponent(b.normal.y) &&
           movementQuantizeContactComponent(a.normal.z) ==
               movementQuantizeContactComponent(b.normal.z);
}

inline bool movementContactsMatchStableIdentity(const MovementContact& a,
                                                const MovementContact& b)
{
    if (!sameMovementLifecycle(a.targetLifecycle, b.targetLifecycle))
        return false;

    if (a.sourceEventId != 0 || b.sourceEventId != 0) {
        return a.sourceEventId != 0 &&
               a.sourceEventId == b.sourceEventId &&
               a.sourceEntityId == b.sourceEntityId &&
               a.source == b.source;
    }

    if (a.contactId != 0 || b.contactId != 0) {
        return a.contactId != 0 &&
               a.contactId == b.contactId &&
               a.kind == b.kind &&
               a.source == b.source &&
               a.sourceEntityId == b.sourceEntityId;
    }

    return false;
}

inline bool movementContactsEquivalent(const MovementContact& a,
                                       const MovementContact& b)
{
    if (movementContactsMatchStableIdentity(a, b))
        return true;
    return movementContactsMatchTickLocalIdentity(a, b);
}

struct MovementContactSet {
    static constexpr std::size_t Capacity = 32;

    std::array<MovementContact, Capacity> items{};
    uint8_t count = 0;
    uint16_t duplicateCount = 0;
    uint16_t overflowCount = 0;

    void clear()
    {
        count = 0;
        duplicateCount = 0;
        overflowCount = 0;
    }

    bool empty() const { return count == 0; }
    std::size_t size() const { return count; }
    const MovementContact* data() const { return items.data(); }
    MovementContact* data() { return items.data(); }
    const MovementContact* begin() const { return items.data(); }
    const MovementContact* end() const { return items.data() + count; }
    MovementContact* begin() { return items.data(); }
    MovementContact* end() { return items.data() + count; }
    const MovementContact& operator[](std::size_t index) const { return items[index]; }
    MovementContact& operator[](std::size_t index) { return items[index]; }

    bool addDeduplicated(const MovementContact& contact)
    {
        for (std::size_t i = 0; i < count; ++i) {
            if (movementContactsEquivalent(items[i], contact)) {
                ++duplicateCount;
                return false;
            }
        }

        if (count < Capacity) {
            items[count++] = contact;
            return true;
        }

        ++overflowCount;
        if (contact.resetsAbilities) {
            for (std::size_t i = 0; i < count; ++i) {
                if (!items[i].resetsAbilities) {
                    items[i] = contact;
                    return true;
                }
            }
        }
        return false;
    }
};

struct MovementContactHistory {
    static constexpr std::size_t Capacity = 64;

    std::array<MovementContact, Capacity> recent{};
    uint8_t count = 0;
    uint8_t nextIndex = 0;
    MovementLifecycleIdentity lifecycle;
    bool lifecycleInitialized = false;

    void clear()
    {
        count = 0;
        nextIndex = 0;
        lifecycle = MovementLifecycleIdentity{};
        lifecycleInitialized = false;
    }

    void resetForLifecycle(MovementLifecycleIdentity nextLifecycle)
    {
        count = 0;
        nextIndex = 0;
        lifecycle = nextLifecycle;
        lifecycleInitialized = true;
    }

    void ensureLifecycle(MovementLifecycleIdentity nextLifecycle)
    {
        if (!lifecycleInitialized || !sameMovementLifecycle(lifecycle, nextLifecycle))
            resetForLifecycle(nextLifecycle);
    }

    bool containsStable(const MovementContact& contact) const
    {
        if (contact.sourceEventId == 0 && contact.contactId == 0)
            return false;
        for (std::size_t i = 0; i < count; ++i) {
            if (movementContactsMatchStableIdentity(recent[i], contact))
                return true;
        }
        return false;
    }

    void recordStable(const MovementContact& contact)
    {
        if (contact.sourceEventId == 0 && contact.contactId == 0)
            return;

        recent[nextIndex] = contact;
        nextIndex = static_cast<uint8_t>((nextIndex + 1) % Capacity);
        if (count < Capacity)
            ++count;
    }
};

struct MovementAbilityResetResult {
    bool airJumpRestored = false;
    bool dashRestored = false;
    bool downDashRestored = false;
    bool freezeRestored = false;
    bool groundReturnRestored = false;
    bool anyRestored = false;
};

struct MovementContactConsumeResult {
    bool consumedAnyContact = false;
    bool appliedReset = false;
    MovementAbilityResetResult reset;
    uint8_t qualifyingContactCount = 0;
    uint8_t ignoredLifecycleCount = 0;
    uint8_t duplicateContactCount = 0;
    uint16_t contactOverflowCount = 0;
};

struct MovementState {
    MovementLifecycleIdentity lifecycle;
    bool movementEnabled = true;

    glm::vec3 position{0.0f};
    glm::vec3 baseVelocity{0.0f};
    glm::vec3 externalImpulse{0.0f};
    // Source-mode impulse shaping (persisted through Player conversion).
    float externalImpulseCarryTimerSeconds = 0.0f;
    float externalImpulseMagnitude = 0.0f;
    glm::vec2 lastInputMoveAxes{0.0f};
    glm::vec2 previousMoveAxes{0.0f};
    float yaw = 0.0f;
    float previousYaw = 0.0f;
    float sizeScale = 1.0f;

    MovementGroundState ground;
    MovementJumpState jump;
    MovementDashState dash;
    MovementDownDashState downDash;
    MovementFreezeState freeze;
    MovementGroundReturnState groundReturn;
    MovementDashMomentumProtectionState dashMomentumProtection;
    MovementContactHistory contactHistory;
    MovementAirDebug airDebug;
};

enum class MovementWalkMode : uint8_t {
    Override = 0,
    Accel = 1,
    Source = 2
};

enum class MovementSpeedCapMode : uint8_t {
    None = 0,
    Hard = 1,
    Soft = 2
};

enum class StationaryCameraInputMode : uint8_t {
    Strict = 0,
    Steering = 1
};

enum class MovementImpulseFrictionMode : uint8_t {
    // Legacy: external impulse decays exponentially every tick via
    // external_impulse_decay and is independent of the movement controllers.
    Exponential = 0,
    // Source style: no exponential decay; the impulse carries through input and
    // is only bled by ground friction (stopspeed-based), like a real knockback.
    Source = 1
};

struct MovementConfig {
    uint32_t simulationHz = 60;
    float fixedDeltaSeconds = 1.0f / 60.0f;
    float maximumDeltaSeconds = 0.033f;

    MovementWalkMode walkMode = MovementWalkMode::Override;
    bool airControlEnabled = true;
    bool bunnyHopEnabled = false;
    bool autoBhopEnabled = true;

    bool preserveStraightSpeed = true;
    float minimumStrafeAngleDegrees = 0.0f;
    float maximumAccelerationPerTick = 0.0f;
    bool diagonalInputNormalization = true;

    bool speedCapEnabled = false;
    MovementSpeedCapMode maximumBhopSpeedMode = MovementSpeedCapMode::None;
    float accelerationFalloffNearCap = 0.0f;
    float landingSpeedRetention = 0.0f;
    bool debugDrawEnabled = false;

    bool requireActiveWishRotation = true;
    StationaryCameraInputMode stationaryCameraInputMode =
        StationaryCameraInputMode::Strict;
    // Air steering responsiveness: how closely the airborne velocity tracks the
    // rotating wish reference (unitless multiplier on the per-tick wish
    // rotation; 1.0 = velocity curves exactly with the camera turn).
    float airSteeringResponse = 1.0f;
    float maximumSteeringDegreesPerSecond = 0.0f;
    float minimumCameraYawDeltaDegrees = 0.25f;
    float minimumWishRotationDegrees = 0.25f;
    float strafeAngularToleranceDegrees = 60.0f;
    float softCapStart = 0.0f;

    float groundSpeed = 0.0f;
    float airSpeed = 0.0f;
    float groundAcceleration = 0.0f;
    float groundDeceleration = 0.0f;
    float groundDirectionChangeResponse = 0.0f;
    float airAcceleration = 0.0f;
    float airMaxWishspeed = 0.0f;
    float airControl = 0.0f;
    // Scale on the residual air speed-gain term in Source mode. 0 = WASD in
    // the air only steers and never adds speed (pure strafing).
    float airSpeedGainMultiplier = 0.0f;
    float stopspeed = 0.0f;
    float bunnyHopSpeedCap = 0.0f;
    float movementSpeedSizeExponent = 0.5f;
    float gravityZ = 0.0f;
    float maximumFallSpeed = 0.0f;

    // ── Source (CS/Quake PM_) movement model ─────────────────────────────
    // Single speed cap used for ground and air (Source sv_maxspeed). 0 = fall
    // back to groundSpeed. Scaled by sizeScale like groundSpeed.
    float sourceMaxSpeed = 0.0f;
    // Linear friction drop (Source sv_friction): drop = max(speed, stopspeed) * friction * dt.
    float sourceFriction = 0.0f;
    // Global surface-friction scalar multiplied into the friction drop (1.0 = normal).
    float surfaceFriction = 1.0f;
    // Landing snap: if downward velocity magnitude is within this epsilon, snap to 0.
    float velocityClipEpsilon = 0.01f;
    bool groundSnap = true;
    // 0..1 fraction of Source friction applied on the landing/autobhop tick.
    // 1.0 = full Source overspeed bleed, 0.0 = preserve overspeed through landings.
    float landingOverspeedBleed = 1.0f;
    // Dash boost protection window in Source mode (seconds).
    float dashGraceSeconds = 0.0f;
    // Friction scale applied to the dash boost while dashGraceTimer is active.
    float dashFrictionMultiplier = 1.0f;

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
    // Source-style impulse shaping.
    MovementImpulseFrictionMode impulseFrictionMode =
        MovementImpulseFrictionMode::Exponential;
    // How long a fresh knockback is untouched before friction starts bleeding it.
    float impulseCarrySeconds = 0.0f;
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
    bool abilitiesReset = false;
    bool airJumpRestored = false;
    bool dashRestored = false;
    bool downDashRestored = false;
    bool freezeRestored = false;
    bool groundReturnRestored = false;
    uint8_t qualifyingContactCount = 0;
    uint8_t dedupedContactCount = 0;
    uint16_t contactOverflowCount = 0;
    MovementContactSet contacts;
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
           movementIsFinite(state.previousMoveAxes) &&
           movementIsFinite(state.yaw) &&
           movementIsFinite(state.previousYaw) &&
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
