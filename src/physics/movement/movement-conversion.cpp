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
#include "config/size-scaling-config.h"
#include "network/server.h"
#include "network/simulation-constants.h"
#include "physics/config.h"

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
    command.jumpHeld = input.jumpHeld;
    command.dashPressed = input.dashPressed;
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
    return state;
}

void applyMovementStateToPlayer(const MovementState& state, Player& player)
{
    player.spawnGeneration = state.lifecycle.spawnGeneration;
    player.pos = state.position;
    player.vel = state.baseVelocity;
    player.externalImpulse = state.externalImpulse;
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
    player.syncLegacyStateToLayers();
}

MovementState movementStateFromServerPlayer(const MimitaNet::ServerPlayer& player)
{
    MovementState state;
    state.lifecycle.spawnGeneration = player.spawnGeneration;
    state.lifecycle.transformEpoch = player.transformEpoch;
    state.movementEnabled = !player.dead && player.spawnState == MimitaNet::ServerPlayer::Active;
    state.position = player.pos;
    state.baseVelocity = player.vel;
    state.externalImpulse = glm::vec3(0.0f);
    state.lastInputMoveAxes = movementClampUnitOrZero(player.input.wish);
    state.yaw = player.yaw;
    state.sizeScale = player.sizeScale;
    state.ground.onGround = player.onGround;
    state.ground.stableOnGround = player.onGround;
    state.ground.hasWorldContact = player.onGround;
    state.dash.dashAvailable = player.dashAvailable;
    state.freeze.heldPreviously = player.input.freezeHeld;
    return state;
}

void applyMovementStateToServerPlayer(const MovementState& state,
                                      MimitaNet::ServerPlayer& player)
{
    player.spawnGeneration = state.lifecycle.spawnGeneration;
    player.transformEpoch = static_cast<uint16_t>(state.lifecycle.transformEpoch);
    player.pos = state.position;
    player.vel = state.baseVelocity;
    player.yaw = state.yaw;
    player.sizeScale = state.sizeScale;
    player.onGround = state.ground.onGround;
    player.dashAvailable = state.dash.dashAvailable;
    player.input.wish = movementClampUnitOrZero(state.lastInputMoveAxes);
}

MovementConfig makeCurrentRuntimeMovementConfig()
{
    MovementConfig config;
    config.simulationHz = MimitaNet::GAMEPLAY_SIMULATION_HZ;
    config.fixedDeltaSeconds = MimitaNet::GAMEPLAY_FIXED_DT;
    config.maximumDeltaSeconds = 0.033f;
    config.groundSpeed = PHYS.moveSpeed;
    config.airSpeed = AIR_SPEED;
    const auto& sizeScaling = SizeScalingConfig::instance().data();
    config.movementSpeedSizeExponent = sizeScaling.movementSpeedExponent;
    config.gravityZ = PHYS.gravity;
    config.maximumFallSpeed = MAX_FALL_SPEED;
    config.jumpVerticalSpeed = PHYS.jumpStrength;
    config.jumpHeightSizeExponent = sizeScaling.jumpHeightExponent;
    config.jumpBufferSeconds = JUMP_BUFFER_TIME;
    config.coyoteSeconds = COYOTE_JUMP_TIME;
    config.maximumAirJumps = AIR_JUMPS_MAX;
    config.groundDashImpulse = DASH_IMPULSE;
    config.airDashImpulse = AIR_DASH_IMPULSE;
    config.dashHorizontalImpulse = AIR_DASH_IMPULSE;
    config.downDashVerticalSpeed = DOWN_DASH_SPEED;
    config.groundReturnVerticalSpeed = GROUND_RETURN_SPEED;
    config.freezeDurationSeconds = FREEZE_MAX_TIME;
    config.freezeCurveExponent = 4.0f;
    config.freezeDashMinimumPassThrough = 0.001f;
    config.maximumExternalImpulseSpeed = MAX_EXTERNAL_IMPULSE_SPEED;
    config.externalImpulseDecay = EXTERNAL_IMPULSE_DECAY;
    config.externalImpulseSteerRate = EXTERNAL_IMPULSE_STEER_RATE;
    config.externalImpulseBrakeRate = EXTERNAL_IMPULSE_BRAKE_RATE;
    config.groundFrictionAmount = GROUND_FRICTION_AMOUNT;
    config.airFrictionAmount = AIR_FRICTION_AMOUNT;
    config.frictionSizeExponent = -0.5f;
    config.almostZeroSpeed = ALMOST_ZERO;
    config.walkableSlopeDot = MAX_WALKABLE_SLOPE_DOT;
    config.collisionSkin = COLLISION_SKIN;
    config.maximumStepHeight = MAX_STEP_HEIGHT;
    config.stableGroundGraceSeconds = 0.08f;
    config.landingMinimumAirborneSeconds = 0.08f;
    config.landingCooldownResetSeconds = 0.3f;
    return config;
}

MovementServerConversionSupport currentServerMovementConversionSupport()
{
    return MovementServerConversionSupport{};
}
