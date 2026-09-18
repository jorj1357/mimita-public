// 07 21 2026, 15 45
/* purpose
* Implements adapters from current Player, input, and server structs to shared movement data.
* Copies only fields that are actually represented by each existing runtime owner.
* Documents server-side movement parity gaps through explicit conversion support flags.
* Does NOT run physics, reconcile prediction, serialize packets, or modify live tick flow.
* Does NOT synthesize missing server timers, external impulses, down-dash, or freeze state.
* Does NOT touch weapon, damage, rendering, audio, or transport behavior.
*/

#include "physics/movement/movement-conversion.h"

#include <cmath>

#include "entities/player.h"
#include "config/movement-config.h"
#include "config/size-scaling-config.h"
#include "hot-reload/hot-movement-policy.h"
#include "hot-reload/hot-movement-presets.h"
#include "live-code/live-behavior.h"
#include "network/server.h"
#include "network/simulation-constants.h"

namespace {

glm::vec3 horizontalForwardFromYaw(float yawDegrees)
{
    constexpr float kPi = 3.14159265358979323846f;
    const float radians = yawDegrees * (kPi / 180.0f);
    return glm::vec3(std::cos(radians), std::sin(radians), 0.0f);
}

glm::vec3 normalizedHorizontalForward(glm::vec3 forward, float fallbackYawDegrees)
{
    forward.z = 0.0f;
    const float lenSq = forward.x * forward.x + forward.y * forward.y;
    if (lenSq <= MOVEMENT_INPUT_EPSILON * MOVEMENT_INPUT_EPSILON)
        return horizontalForwardFromYaw(fallbackYawDegrees);
    return forward / std::sqrt(lenSq);
}

glm::vec2 chooseInputAxes(const InputFrame& frame, const InputState& input)
{
    if (input.wishMoveXY.x != 0.0f || input.wishMoveXY.y != 0.0f)
        return movementClampUnitOrZero(input.wishMoveXY);
    return movementClampUnitOrZero(glm::vec2(frame.moveX, frame.moveY));
}

// One tuning authority: ask the hot movement module for a preset's tuning.
// Falls back to the same single registry definition (compiled into the EXE)
// when no module handled the event, so there is still exactly one source of
// truth for the values.
GameMovementTuningV1 requestMovementTuning(MimitaHotMovement::MovementPresetId id)
{
    GameMovementTuningV1 t{};
    t.presetId = static_cast<std::uint32_t>(id);
    if (LiveBehavior::dispatchGameplayEvent64(GAME_EVENT_MOVEMENT_TUNING, &t,
                                              sizeof(t), 0, 0, 0) &&
        t.handled)
        return t;
    return MimitaHotMovement::getMovementPreset(id).tuning;
}

} // namespace

MovementCommand movementCommandFromInput(const InputFrame& frame,
                                         const InputState& input,
                                         uint32_t sequence,
                                         uint64_t clientSimulationTick,
                                         MovementLifecycleIdentity lifecycle)
{
    MovementCommand command;
    command.sequence = sequence;
    command.clientSimulationTick = clientSimulationTick;
    command.lifecycle = lifecycle;
    command.moveAxes = chooseInputAxes(frame, input);
    command.horizontalCameraForward = normalizedHorizontalForward(input.camForward, frame.lookYaw);
    command.lookYaw = frame.lookYaw;
    command.lookPitch = frame.lookPitch;
    command.jumpHeld = input.jumpHeld || frame.jump;
    command.jumpPressed = input.jumpPressed || frame.jumpPressed;
    command.dashPressed = input.dashPressed || frame.dashPressed;
    command.downDashPressed = input.downDashPressed || frame.downDashPressed;
    command.groundReturnPressed = input.groundReturnPressed || frame.groundReturnPressed;
    command.freezeHeld = input.freezeHeld || frame.freezeHeld;
    command.freezePressed = input.freezePressed || frame.freezePressed;

    command.movementDirectionPressed =
        input.movementPressed || frame.movementPressed ||
        command.moveAxes.x != 0.0f || command.moveAxes.y != 0.0f;
    command.movementDirectionFreshPressed =
        input.movementJustPressed || frame.movementJustPressed;
    command.movementDirectionReleased =
        !command.movementDirectionPressed && input.movementPressed;
    command.movementDirectionChanged = command.movementDirectionFreshPressed;
    command.movementHeldDurationSeconds = input.movementHeldDuration;
    return command;
}

MovementCommand movementCommandFromServerInput(const MimitaNet::ServerInput& input,
                                               uint32_t sequence,
                                               MovementLifecycleIdentity lifecycle)
{
    MovementCommand command;
    command.sequence = sequence;
    command.clientSimulationTick = input.tick;
    command.lifecycle = lifecycle;
    command.moveAxes = movementClampUnitOrZero(input.wish);
    command.horizontalCameraForward = normalizedHorizontalForward(input.camForward, input.yaw);
    command.lookYaw = input.yaw;
    command.lookPitch = input.lookPitch;
    command.jumpHeld = input.jumpHeld;
    command.dashPressed = input.dashPressed;
    command.downDashPressed = input.downDashPressed;
    command.freezeHeld = input.freezeHeld;
    command.movementDirectionPressed =
        command.moveAxes.x != 0.0f || command.moveAxes.y != 0.0f;
    return command;
}

MovementState movementStateFromPlayer(const Player& player,
                                      MovementLifecycleIdentity lifecycle)
{
    MovementState state;
    state.lifecycle = lifecycle;
    state.movementEnabled = !player.dead;
    state.position = player.pos;
    state.baseVelocity = player.vel;
    state.externalImpulse = player.externalImpulse;
    state.externalImpulseCarryTimerSeconds = player.externalImpulseCarryTimer;
    state.externalImpulseMagnitude = player.externalImpulseMagnitude;
    state.airDebug = player.airDebug;
    state.lastInputMoveAxes = movementClampUnitOrZero(player.inputWishMove);
    state.yaw = player.yaw;
    state.sizeScale = player.sizeScale;

    state.ground.onGround = player.ground.onGround;
    state.ground.stableOnGround = player.ground.stableOnGround;
    state.ground.wasOnGround = player.ground.wasOnGround;
    state.ground.hasWorldContact = player.ground.hasWorldContact;
    state.ground.realWorldContactThisFrame = player.ground.realWorldContactThisFrame;
    state.ground.didLand = player.ground.didLand;
    state.ground.groundLostTimerSeconds = player.ground.groundLostTimer;
    state.ground.airborneTimerSeconds = player.ground.airborneTimer;
    state.ground.landingAirborneDurationSeconds = player.ground.landingAirborneDuration;
    state.ground.landingImpactSpeed = player.ground.landingImpactSpeed;
    state.ground.landingCooldownSeconds = player.ground.landingCooldown;
    state.ground.worldContactLostTimerSeconds = player.ground.worldContactLostTimer;

    state.jump.airJumpsLeft = player.jump.airJumpsLeft;
    state.jump.jumpHeldPreviously = player.jump.jumpHeldPrev;
    state.jump.airJumpLocked = player.jump.airJumpLocked;
    state.jump.airJumpArmed = player.jump.airJumpArmed;
    state.jump.jumpIntentTimerSeconds = player.jump.jumpIntentTimer;
    state.jump.coyoteTimerSeconds = player.jump.coyoteTimer;
    state.jump.didGroundJump = player.jump.didGroundJump;
    state.jump.didAirJump = player.jump.didAirJump;

    state.dash.dashAvailable = player.dash.dashAvailable;
    state.dash.dashHeldPreviously = player.dash.dashHeldPrev;
    state.dash.moveHeldPreviously = player.dash.moveHeldPrev;
    state.dash.dashMovementTicks = player.dash.dashMovementTicks;
    state.dash.lastDashQuality = player.dash.lastDashQuality;
    state.dash.didDash = player.dash.didDash;
    state.dash.frictionOverride = player.dash.frictionOverride;
    state.dash.tickPerfectDash = player.dash.tickPerfectDash;
    state.dash.dashGraceTimerSeconds = player.dash.dashGraceTimer;
    state.dashMomentumProtection.active = player.dash.momentumProtectionActive;
    state.dashMomentumProtection.protectedMoveAxes =
        movementClampUnitOrZero(player.dash.momentumProtectedMoveAxes);
    state.dashMomentumProtection.usedCameraForwardFallback =
        player.dash.momentumProtectionUsedCameraForwardFallback;
    state.dashMomentumProtection.movementInputGeneration =
        player.dash.movementInputGeneration;

    state.downDash.available = player.dash.downDashAvailable;
    state.downDash.didDownDash = player.dash.didDownDash;

    state.freeze.available = player.freeze.freezeAvailable;
    state.freeze.heldPreviously = player.freeze.freezeHeldPrev;
    state.freeze.active = player.freeze.freezeActive;
    state.freeze.timerSeconds = player.freeze.freezeTimer;
    state.freeze.didFreeze = player.freeze.didFreeze;

    state.groundReturn.available = player.groundReturn.available;
    state.groundReturn.charges = player.groundReturn.charges;
    state.groundReturn.rechargeTimerSeconds = player.groundReturn.rechargeTimer;
    state.contactHistory = player.movementContactHistory;
    return state;
}

void applyMovementStateToPlayer(const MovementState& state, Player& player)
{
    player.spawnGeneration = state.lifecycle.spawnGeneration;
    player.pos = state.position;
    player.vel = state.baseVelocity;
    player.externalImpulse = state.externalImpulse;
    player.externalImpulseCarryTimer = state.externalImpulseCarryTimerSeconds;
    player.externalImpulseMagnitude = state.externalImpulseMagnitude;
    player.airDebug = state.airDebug;
    player.inputWishMove = movementClampUnitOrZero(state.lastInputMoveAxes);
    player.yaw = state.yaw;
    player.sizeScale = state.sizeScale;

    player.ground.onGround = state.ground.onGround;
    player.ground.stableOnGround = state.ground.stableOnGround;
    player.ground.wasOnGround = state.ground.wasOnGround;
    player.ground.hasWorldContact = state.ground.hasWorldContact;
    player.ground.realWorldContactThisFrame = state.ground.realWorldContactThisFrame;
    player.ground.didLand = state.ground.didLand;
    player.ground.groundLostTimer = state.ground.groundLostTimerSeconds;
    player.ground.airborneTimer = state.ground.airborneTimerSeconds;
    player.ground.landingAirborneDuration = state.ground.landingAirborneDurationSeconds;
    player.ground.landingImpactSpeed = state.ground.landingImpactSpeed;
    player.ground.landingCooldown = state.ground.landingCooldownSeconds;
    player.ground.worldContactLostTimer = state.ground.worldContactLostTimerSeconds;

    player.jump.airJumpsLeft = state.jump.airJumpsLeft;
    player.jump.jumpHeldPrev = state.jump.jumpHeldPreviously;
    player.jump.airJumpLocked = state.jump.airJumpLocked;
    player.jump.airJumpArmed = state.jump.airJumpArmed;
    player.jump.jumpIntentTimer = state.jump.jumpIntentTimerSeconds;
    player.jump.coyoteTimer = state.jump.coyoteTimerSeconds;
    player.jump.didGroundJump = state.jump.didGroundJump;
    player.jump.didAirJump = state.jump.didAirJump;

    player.dash.dashAvailable = state.dash.dashAvailable;
    player.dash.dashHeldPrev = state.dash.dashHeldPreviously;
    player.dash.moveHeldPrev = state.dash.moveHeldPreviously;
    player.dash.dashMovementTicks = state.dash.dashMovementTicks;
    player.dash.lastDashQuality = state.dash.lastDashQuality;
    player.dash.didDash = state.dash.didDash;
    player.dash.frictionOverride = state.dash.frictionOverride;
    player.dash.tickPerfectDash = state.dash.tickPerfectDash;
    player.dash.dashGraceTimer = state.dash.dashGraceTimerSeconds;
    player.dash.momentumProtectionActive = state.dashMomentumProtection.active;
    player.dash.momentumProtectedMoveAxes =
        movementClampUnitOrZero(state.dashMomentumProtection.protectedMoveAxes);
    player.dash.momentumProtectionUsedCameraForwardFallback =
        state.dashMomentumProtection.usedCameraForwardFallback;
    player.dash.movementInputGeneration =
        state.dashMomentumProtection.movementInputGeneration;
    player.dash.downDashAvailable = state.downDash.available;
    player.dash.didDownDash = state.downDash.didDownDash;

    player.freeze.freezeAvailable = state.freeze.available;
    player.freeze.freezeHeldPrev = state.freeze.heldPreviously;
    player.freeze.freezeActive = state.freeze.active;
    player.freeze.freezeTimer = state.freeze.timerSeconds;
    player.freeze.didFreeze = state.freeze.didFreeze;

    player.groundReturn.available = state.groundReturn.available;
    player.groundReturn.charges = state.groundReturn.charges;
    player.groundReturn.rechargeTimer = state.groundReturn.rechargeTimerSeconds;
    player.movementContactHistory = state.contactHistory;
    player.syncLegacyStateToLayers();
}

MovementState movementStateFromServerPlayer(const MimitaNet::ServerPlayer& player)
{
    MovementState state = player.movement;
    state.lifecycle.spawnGeneration = player.spawnGeneration;
    state.lifecycle.transformEpoch = player.transformEpoch;
    state.movementEnabled = !player.dead && player.spawnState == MimitaNet::ServerPlayer::Active;
    state.position = player.pos;
    if (!movementIsFinite(state.baseVelocity) ||
        !movementIsFinite(state.externalImpulse) ||
        glm::length((state.baseVelocity + state.externalImpulse) - player.vel) > 0.25f)
    {
        if (movementIsFinite(state.externalImpulse))
            state.baseVelocity = player.vel - state.externalImpulse;
        else
        {
            state.baseVelocity = player.vel;
            state.externalImpulse = glm::vec3(0.0f);
        }
    }
    state.lastInputMoveAxes = movementClampUnitOrZero(player.input.wish);
    state.yaw = player.yaw;
    state.sizeScale = player.sizeScale;
    state.ground.onGround = player.onGround;
    state.ground.stableOnGround = player.onGround;
    state.ground.hasWorldContact = player.onGround;
    state.dash.dashAvailable = player.dashAvailable;
    state.downDash.available = player.movement.downDash.available;
    state.freeze.available = player.movement.freeze.available;
    state.freeze.active = player.movement.freeze.active;
    state.freeze.heldPreviously = player.input.freezeHeld;
    return state;
}

void applyMovementStateToServerPlayer(const MovementState& state,
                                      MimitaNet::ServerPlayer& player)
{
    player.spawnGeneration = state.lifecycle.spawnGeneration;
    player.transformEpoch = static_cast<uint16_t>(state.lifecycle.transformEpoch);
    player.movement = state;
    player.pos = state.position;
    player.vel = state.baseVelocity + state.externalImpulse;
    player.yaw = state.yaw;
    player.sizeScale = state.sizeScale;
    player.onGround = state.ground.onGround;
    player.dashAvailable = state.dash.dashAvailable;
    player.input.wish = movementClampUnitOrZero(state.lastInputMoveAxes);
}

MovementConfig applyRuntimeMovementTuning(MovementConfig config)
{
    config.simulationHz = MimitaNet::GAMEPLAY_SIMULATION_HZ;
    config.fixedDeltaSeconds = MimitaNet::GAMEPLAY_FIXED_DT;
    const auto& sizeScaling = SizeScalingConfig::instance().data();
    config.movementSpeedSizeExponent = sizeScaling.movementSpeedExponent;
    config.jumpHeightSizeExponent = sizeScaling.jumpHeightExponent;
    return config;
}

MovementConfig makeMovementConfigForPreset(std::uint32_t presetId)
{
    // Single C++ movement authority. The hot module owns the edit-able preset
    // registry; JSON presets are reference/comparison material only.
    const MimitaHotMovement::MovementPresetId id =
        presetId < MimitaHotMovement::kMovementPresetCount
            ? static_cast<MimitaHotMovement::MovementPresetId>(presetId)
            : MimitaHotMovement::kActiveMovementPreset;
    const GameMovementTuningV1 t = requestMovementTuning(id);
    MovementConfig config = movementRuntimeDefaults();

    config.walkMode = t.walkMode == MimitaHotMovement::kWalkModeSource
                          ? MovementWalkMode::Source
                          : (t.walkMode == MimitaHotMovement::kWalkModeAccel
                                 ? MovementWalkMode::Accel
                                 : MovementWalkMode::Override);
    config.airControlEnabled = t.airControlEnabled != 0;
    config.bunnyHopEnabled = t.bunnyHopEnabled != 0;
    config.autoBhopEnabled = t.autoBhopEnabled != 0;
    config.preserveStraightSpeed = t.preserveStraightSpeed != 0;
    config.minimumStrafeAngleDegrees = t.minimumStrafeAngleDegrees;
    config.maximumAccelerationPerTick = t.maximumAccelerationPerTick;
    config.diagonalInputNormalization = t.diagonalInputNormalization != 0;
    config.speedCapEnabled = t.speedCapEnabled != 0;
    config.maximumBhopSpeedMode =
        static_cast<MovementSpeedCapMode>(t.maximumBhopSpeedMode);
    config.accelerationFalloffNearCap = t.accelerationFalloffNearCap;
    config.speedLimitEnabled = t.speedLimitEnabled != 0;
    config.speedLimit = t.speedLimit;
    config.speedLimitMode = static_cast<MovementSpeedLimitMode>(t.speedLimitMode);
    config.landingSpeedRetention = t.landingSpeedRetention;
    config.debugDrawEnabled = t.debugDrawEnabled != 0;
    config.requireActiveWishRotation = t.requireActiveWishRotation != 0;
    config.stationaryCameraInputMode =
        static_cast<StationaryCameraInputMode>(t.stationaryCameraInputMode);
    config.airSteeringResponse = t.airSteeringResponse;
    config.maximumSteeringDegreesPerSecond = t.maximumSteeringDegreesPerSecond;
    config.minimumCameraYawDeltaDegrees = t.minimumCameraYawDeltaDegrees;
    config.minimumWishRotationDegrees = t.minimumWishRotationDegrees;
    config.strafeAngularToleranceDegrees = t.strafeAngularToleranceDegrees;
    config.softCapStart = t.softCapStart;

    config.groundSpeed = t.groundSpeed;
    config.airSpeed = t.airSpeed;
    config.sourceMaxSpeed = t.sourceMaxSpeed;
    config.sourceFriction = t.sourceFriction;
    config.groundAcceleration = t.groundAcceleration;
    config.groundDeceleration = t.groundDeceleration;
    config.groundDirectionChangeResponse = t.groundDirectionChangeResponse;
    config.airAcceleration = t.airAcceleration;
    config.airMaxWishspeed = t.airMaxWishspeed;
    config.sourceAirAccelerateBugCompatible =
        t.sourceAirAccelerateBugCompatible != 0;
    config.airControl = t.airControl;
    config.airSpeedGainMultiplier = t.airSpeedGainMultiplier;
    config.airInputBlendingEnabled = t.airInputBlendingEnabled != 0;
    config.airInputBlending = t.airInputBlending;
    config.airInputMouseThresholdDegrees = t.airInputMouseThresholdDegrees;
    config.stopspeed = t.stopspeed;
    config.bunnyHopSpeedCap = t.bunnyHopSpeedCap;
    config.gravityZ = -t.gravityMagnitude;
    config.maximumFallSpeed = t.maxFallSpeed;
    config.surfaceFriction = t.surfaceFriction;
    config.velocityClipEpsilon = t.velocityClipEpsilon;
    config.groundSnap = t.groundSnap != 0;
    config.landingOverspeedBleed = t.landingOverspeedBleed;
    config.dashGraceSeconds = t.dashGraceSeconds;
    config.dashFrictionMultiplier = t.dashFrictionMultiplier;
    config.jumpVerticalSpeed = t.jumpSpeed;
    config.jumpBufferSeconds = t.jumpBufferSeconds;
    config.coyoteSeconds = t.coyoteSeconds;
    config.maximumAirJumps = static_cast<int>(t.maximumAirJumps);
    config.dashEnabled = t.dashEnabled != 0;
    config.downDashEnabled = t.downDashEnabled != 0;
    config.freezeEnabled = t.freezeEnabled != 0;
    config.groundDashImpulse = t.groundDashImpulse;
    config.airDashImpulse = t.airDashImpulse;
    config.downDashVerticalSpeed = t.downDashSpeed;
    config.freezeDurationSeconds = t.freezeDurationSeconds;
    config.freezeCurveExponent = t.freezeCurveExponent;
    config.maximumExternalImpulseSpeed = t.maximumExternalImpulseSpeed;
    config.externalImpulseDecay = t.externalImpulseDecay;
    config.impulseCarrySeconds = t.impulseCarrySeconds;
    config.impulseFrictionMode =
        t.impulseFrictionMode == 1u ? MovementImpulseFrictionMode::Source
                                    : MovementImpulseFrictionMode::Exponential;
    config.groundFrictionAmount = t.groundFrictionAmount;
    config.airFrictionAmount = t.airFrictionAmount;
    return applyRuntimeMovementTuning(config);
}

MovementConfig makeCurrentRuntimeMovementConfig()
{
    return makeMovementConfigForPreset(
        static_cast<std::uint32_t>(MimitaHotMovement::kActiveMovementPreset));
}

MovementServerConversionSupport currentServerMovementConversionSupport()
{
    MovementServerConversionSupport support;
    support.externalImpulse = true;
    support.detailedGroundTimers = true;
    support.jumpTimers = true;
    support.airJumpState = true;
    support.downDashState = true;
    support.freezeTimerState = true;
    support.groundReturnState = true;
    support.dashMomentumProtection = true;
    return support;
}
