// 07 21 2026, 17 25
/* purpose
* Verifies the shared Stage 2A walking, gravity, jump, and external impulse kernel.
* Compares shared MovementState simulation against the current local Player basic slice.
* Covers deterministic helper behavior, contact reset phase ordering, and randomized parity.
* Does NOT launch mimita.exe, render, play audio, send packets, or require networking.
* Does NOT test dash, down dash, freeze, Ground Return, projectiles, weapons, or collision sweeps.
* Does NOT change runtime authority or packet layout.
*/

#include "physics/movement/movement-step.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <string>

#include "combat/weapon-runtime.h"
#include "config/player-settings.h"
#include "entities/player.h"
#include "physics/config.h"
#include "physics/movement/movement-conversion.h"
#include "physics/movement/physics-friction.h"
#include "physics/movement/physics-gravity.h"
#include "physics/movement/physics-jump.h"
#include "physics/movement/physics-walk.h"

PlayerSettings& GetPlayerSettings()
{
    static PlayerSettings settings;
    return settings;
}

bool LoadPlayerSettings(const std::string&)
{
    return true;
}

bool SavePlayerSettings(const std::string&)
{
    return true;
}

void resetAllWeaponRuntimesForSpawn(Player&, const char*)
{
}

Player::Player()
    : Player(false)
{
}

Player::Player(bool)
{
}

void Player::reset()
{
}

bool Player::loadModel(const char*)
{
    return false;
}

bool Player::loadCharacter(const std::string&)
{
    return false;
}

void Player::syncLegacyStateToLayers()
{
    origin.position = pos;
    movementCapsule.position = pos;
    movementCapsule.velocity = vel;
}

namespace {

constexpr uint32_t kRandomSeed = 0x2A2026u;
constexpr float kDt = 1.0f / 60.0f;
constexpr float kTolerance = 0.0002f;

int gFailures = 0;

void fail(const std::string& message)
{
    std::cerr << "[FAIL] " << message << "\n";
    ++gFailures;
}

void check(bool condition, const std::string& message)
{
    if (!condition)
        fail(message);
}

void checkNear(float actual, float expected, float tolerance, const std::string& message)
{
    if (std::fabs(actual - expected) > tolerance) {
        std::ostringstream out;
        out << message << " expected=" << expected << " actual=" << actual;
        fail(out.str());
    }
}

void checkVec2(glm::vec2 actual, glm::vec2 expected, const std::string& message)
{
    checkNear(actual.x, expected.x, kTolerance, message + ".x");
    checkNear(actual.y, expected.y, kTolerance, message + ".y");
}

void checkVec3(glm::vec3 actual, glm::vec3 expected, const std::string& message)
{
    checkNear(actual.x, expected.x, kTolerance, message + ".x");
    checkNear(actual.y, expected.y, kTolerance, message + ".y");
    checkNear(actual.z, expected.z, kTolerance, message + ".z");
}

MovementConfig currentConfig()
{
    return makeCurrentRuntimeMovementConfig();
}

MovementCommand commandFor(glm::vec2 axes = glm::vec2(0.0f),
                           bool jumpHeld = false,
                           bool jumpPressed = false)
{
    MovementCommand command;
    command.moveAxes = axes;
    command.jumpHeld = jumpHeld;
    command.jumpPressed = jumpPressed;
    command.movementDirectionPressed = movementHasMoveInput(axes);
    return command;
}

MovementCollisionFeedback collisionFor(bool onGround)
{
    MovementCollisionFeedback collision;
    collision.onGround = onGround;
    collision.hasWorldContact = onGround;
    collision.realWorldContactThisFrame = onGround;
    return collision;
}

MovementState defaultState(bool onGround = false)
{
    MovementState state;
    state.lifecycle = MovementLifecycleIdentity{10, 20};
    state.sizeScale = 1.0f;
    state.ground.onGround = onGround;
    state.ground.stableOnGround = onGround;
    state.ground.wasOnGround = onGround;
    state.jump.airJumpsLeft = AIR_JUMPS_MAX;
    state.dash.frictionOverride = 1.0f;
    return state;
}

void legacyBasicStep(Player& player,
                     const MovementCommand& command,
                     const MovementCollisionFeedback& collision,
                     const MovementConfig& config,
                     float dt,
                     MovementStepEvents* events)
{
    dt = movementClampStepDelta(dt, config);
    player.inputWishMove = movementClampUnitOrZero(command.moveAxes);

    player.jump.didGroundJump = false;
    player.jump.didAirJump = false;
    player.dash.didDash = false;
    player.dash.didDownDash = false;
    player.ground.didLand = false;
    player.freeze.didFreeze = false;

    doGravity(player, dt);

    const bool previousOnGround = player.ground.onGround;
    const bool previousStableOnGround = player.ground.stableOnGround;

    player.ground.onGround = collision.onGround;
    if (collision.onGround)
        player.ground.groundLostTimer = 0.0f;
    else
        player.ground.groundLostTimer += dt;

    player.ground.stableOnGround =
        collision.onGround ||
        player.ground.groundLostTimer < config.stableGroundGraceSeconds;
    player.ground.hasWorldContact = collision.hasWorldContact;
    player.ground.realWorldContactThisFrame = collision.realWorldContactThisFrame;

    if (collision.onGround) {
        player.jump.airJumpsLeft = config.maximumAirJumps;
        player.jump.airJumpArmed = true;
        player.jump.airJumpLocked = false;
        player.dash.dashAvailable = true;
        player.groundReturn.available = true;
        player.dash.downDashAvailable = true;
        player.freeze.freezeAvailable = true;
    }

    if (!collision.onGround && command.movementDirectionPressed) {
        if (player.dash.dashMovementTicks < 99)
            ++player.dash.dashMovementTicks;
    } else {
        player.dash.dashMovementTicks = 0;
    }

    if (command.movementDirectionPressed || command.dashPressed || command.freezeHeld) {
        const float upZ = player.externalImpulse.z > 0.0f ? player.externalImpulse.z : 0.0f;
        player.externalImpulse = glm::vec3(0.0f);
        player.externalImpulse.z = upZ;
    }

    if (command.movementDirectionPressed && player.dash.frictionOverride >= 1.0f)
        doWalk(player, command.moveAxes, collision.onGround, dt);

    doJump(player, command.jumpHeld, command.jumpPressed, dt);

    if (player.dash.frictionOverride < 1.0f && !player.dash.didDash) {
        const bool inputDetected =
            command.movementDirectionFreshPressed ||
            command.dashPressed ||
            command.freezeHeld ||
            command.downDashPressed;
        const bool abilityUsed = player.dash.didDownDash || player.freeze.didFreeze;
        if (inputDetected || abilityUsed) {
            player.dash.frictionOverride = 1.0f;
            player.dash.tickPerfectDash = false;
        }
    }

    doFriction(player, player.ground.stableOnGround, dt);

    const float previousAirborneSeconds = player.ground.airborneTimer;
    if (player.ground.stableOnGround)
        player.ground.airborneTimer = 0.0f;
    else
        player.ground.airborneTimer += dt;

    player.ground.landingCooldown =
        std::max(0.0f, player.ground.landingCooldown - dt);
    const bool stableLanding =
        !previousStableOnGround && player.ground.stableOnGround;
    if (stableLanding &&
        previousAirborneSeconds > config.landingMinimumAirborneSeconds &&
        player.ground.landingCooldown <= 0.0f) {
        player.ground.didLand = true;
        player.ground.landingCooldown = config.landingCooldownResetSeconds;
    }

    player.ground.wasOnGround = player.ground.stableOnGround;

    if (events) {
        events->didGroundJump = player.jump.didGroundJump;
        events->didAirJump = player.jump.didAirJump;
        events->leftGround = previousOnGround && !collision.onGround;
        events->didLand = player.ground.didLand;
        events->touchedGround = collision.onGround;
    }
}

void compareBool(bool shared, bool legacy, const std::string& field, const std::string& context)
{
    if (shared != legacy)
        fail(context + " field=" + field + " shared=" + std::to_string(shared) +
             " legacy=" + std::to_string(legacy));
}

void compareInt(int shared, int legacy, const std::string& field, const std::string& context)
{
    if (shared != legacy)
        fail(context + " field=" + field + " shared=" + std::to_string(shared) +
             " legacy=" + std::to_string(legacy));
}

void compareFloat(float shared,
                  float legacy,
                  const std::string& field,
                  const std::string& context,
                  float tolerance = kTolerance)
{
    if (std::fabs(shared - legacy) > tolerance) {
        std::ostringstream out;
        out << context << " field=" << field << " shared=" << shared
            << " legacy=" << legacy << " diff=" << (shared - legacy);
        fail(out.str());
    }
}

void compareRelevantState(const MovementState& shared,
                          const MovementState& legacy,
                          const std::string& context)
{
    compareFloat(shared.position.x, legacy.position.x, "position.x", context);
    compareFloat(shared.position.y, legacy.position.y, "position.y", context);
    compareFloat(shared.position.z, legacy.position.z, "position.z", context);
    compareFloat(shared.baseVelocity.x, legacy.baseVelocity.x, "baseVelocity.x", context);
    compareFloat(shared.baseVelocity.y, legacy.baseVelocity.y, "baseVelocity.y", context);
    compareFloat(shared.baseVelocity.z, legacy.baseVelocity.z, "baseVelocity.z", context);
    compareFloat(shared.externalImpulse.x, legacy.externalImpulse.x, "externalImpulse.x", context);
    compareFloat(shared.externalImpulse.y, legacy.externalImpulse.y, "externalImpulse.y", context);
    compareFloat(shared.externalImpulse.z, legacy.externalImpulse.z, "externalImpulse.z", context);
    compareFloat(shared.lastInputMoveAxes.x, legacy.lastInputMoveAxes.x, "lastInputMoveAxes.x", context);
    compareFloat(shared.lastInputMoveAxes.y, legacy.lastInputMoveAxes.y, "lastInputMoveAxes.y", context);

    compareBool(shared.ground.onGround, legacy.ground.onGround, "ground.onGround", context);
    compareBool(shared.ground.stableOnGround, legacy.ground.stableOnGround, "ground.stableOnGround", context);
    compareBool(shared.ground.wasOnGround, legacy.ground.wasOnGround, "ground.wasOnGround", context);
    compareBool(shared.ground.hasWorldContact, legacy.ground.hasWorldContact, "ground.hasWorldContact", context);
    compareBool(shared.ground.realWorldContactThisFrame, legacy.ground.realWorldContactThisFrame, "ground.realWorldContactThisFrame", context);
    compareBool(shared.ground.didLand, legacy.ground.didLand, "ground.didLand", context);
    compareFloat(shared.ground.groundLostTimerSeconds, legacy.ground.groundLostTimerSeconds, "ground.groundLostTimerSeconds", context);
    compareFloat(shared.ground.airborneTimerSeconds, legacy.ground.airborneTimerSeconds, "ground.airborneTimerSeconds", context);
    compareFloat(shared.ground.landingCooldownSeconds, legacy.ground.landingCooldownSeconds, "ground.landingCooldownSeconds", context);

    compareInt(shared.jump.airJumpsLeft, legacy.jump.airJumpsLeft, "jump.airJumpsLeft", context);
    compareBool(shared.jump.jumpHeldPreviously, legacy.jump.jumpHeldPreviously, "jump.jumpHeldPreviously", context);
    compareBool(shared.jump.airJumpLocked, legacy.jump.airJumpLocked, "jump.airJumpLocked", context);
    compareBool(shared.jump.airJumpArmed, legacy.jump.airJumpArmed, "jump.airJumpArmed", context);
    compareFloat(shared.jump.jumpIntentTimerSeconds, legacy.jump.jumpIntentTimerSeconds, "jump.jumpIntentTimerSeconds", context);
    compareFloat(shared.jump.coyoteTimerSeconds, legacy.jump.coyoteTimerSeconds, "jump.coyoteTimerSeconds", context);
    compareBool(shared.jump.didGroundJump, legacy.jump.didGroundJump, "jump.didGroundJump", context);
    compareBool(shared.jump.didAirJump, legacy.jump.didAirJump, "jump.didAirJump", context);

    compareBool(shared.dash.dashAvailable, legacy.dash.dashAvailable, "dash.dashAvailable", context);
    compareInt(shared.dash.dashMovementTicks, legacy.dash.dashMovementTicks, "dash.dashMovementTicks", context);
    compareBool(shared.dash.didDash, legacy.dash.didDash, "dash.didDash", context);
    compareFloat(shared.dash.frictionOverride, legacy.dash.frictionOverride, "dash.frictionOverride", context);
    compareBool(shared.dash.tickPerfectDash, legacy.dash.tickPerfectDash, "dash.tickPerfectDash", context);
    compareBool(shared.downDash.available, legacy.downDash.available, "downDash.available", context);
    compareBool(shared.downDash.didDownDash, legacy.downDash.didDownDash, "downDash.didDownDash", context);
    compareBool(shared.freeze.available, legacy.freeze.available, "freeze.available", context);
    compareBool(shared.freeze.didFreeze, legacy.freeze.didFreeze, "freeze.didFreeze", context);
    compareBool(shared.groundReturn.available, legacy.groundReturn.available, "groundReturn.available", context);
}

void compareEvents(const MovementStepEvents& shared,
                   const MovementStepEvents& legacy,
                   const std::string& context)
{
    compareBool(shared.didGroundJump, legacy.didGroundJump, "events.didGroundJump", context);
    compareBool(shared.didAirJump, legacy.didAirJump, "events.didAirJump", context);
    compareBool(shared.leftGround, legacy.leftGround, "events.leftGround", context);
    compareBool(shared.didLand, legacy.didLand, "events.didLand", context);
    compareBool(shared.touchedGround, legacy.touchedGround, "events.touchedGround", context);
}

void runSharedVsLegacyCase(const MovementState& initial,
                           const MovementCommand& command,
                           const MovementCollisionFeedback& collision,
                           const std::string& context)
{
    const MovementConfig config = currentConfig();

    Player legacyPlayer(false);
    applyMovementStateToPlayer(initial, legacyPlayer);
    MovementStepEvents legacyEvents;
    legacyBasicStep(legacyPlayer, command, collision, config, kDt, &legacyEvents);
    MovementState legacyState = movementStateFromPlayer(legacyPlayer, initial.lifecycle);

    MovementState sharedState = initial;
    MovementStepResult sharedResult =
        simulateBasicMovementStep(sharedState, command, config, collision, kDt);

    compareRelevantState(sharedResult.state, legacyState, context);
    compareEvents(sharedResult.events, legacyEvents, context);
}

void testWalking()
{
    const MovementConfig config = currentConfig();

    checkVec2(movementWalkVelocityXY(glm::vec2(0.0f), glm::vec2(1.0f, 0.0f), true, 1.0f, config),
              glm::vec2(20.0f, 0.0f), "grounded W speed");
    checkVec2(movementWalkVelocityXY(glm::vec2(0.0f), glm::vec2(1.0f, 0.0f), false, 1.0f, config),
              glm::vec2(20.0f, 0.0f), "airborne W speed");
    const float diag = 20.0f / std::sqrt(2.0f);
    checkVec2(movementWalkVelocityXY(glm::vec2(0.0f), glm::vec2(1.0f, 1.0f), true, 1.0f, config),
              glm::vec2(diag, diag), "diagonal normalized");
    checkVec2(movementWalkVelocityXY(glm::vec2(0.0f), glm::vec2(-1.0f, 0.0f), true, 1.0f, config),
              glm::vec2(-20.0f, 0.0f), "S movement");
    checkVec2(movementWalkVelocityXY(glm::vec2(0.0f), glm::vec2(0.0f, -1.0f), true, 1.0f, config),
              glm::vec2(0.0f, -20.0f), "A movement");
    checkVec2(movementWalkVelocityXY(glm::vec2(0.0f), glm::vec2(0.0f, 1.0f), true, 1.0f, config),
              glm::vec2(0.0f, 20.0f), "D movement");
    checkVec2(movementWalkVelocityXY(glm::vec2(5.0f, -7.0f), glm::vec2(0.0f), true, 1.0f, config),
              glm::vec2(5.0f, -7.0f), "zero input preserves XY");

    glm::vec3 velocity(80.0f, -5.0f, 37.0f);
    movementApplyWalkVelocity(velocity, glm::vec2(1.0f, 0.0f), true, 1.0f,
                              config.groundSpeed, config.airSpeed,
                              config.movementSpeedSizeExponent);
    checkVec3(velocity, glm::vec3(20.0f, 0.0f, 37.0f), "WASD preserves Z and overwrites XY");
    checkNear(movementScaledGroundSpeed(config, 4.0f), 40.0f, kTolerance,
              "size-scaled walk speed");
}

void testGravity()
{
    const MovementConfig config = currentConfig();
    const float first = movementApplyGravityZ(0.0f, config.gravityZ, config.maximumFallSpeed, kDt);
    checkNear(first, -58.0f / 60.0f, kTolerance, "gravity one tick from zero");

    float z = 0.0f;
    for (int i = 0; i < 10; ++i)
        z = movementApplyGravityZ(z, config.gravityZ, config.maximumFallSpeed, kDt);
    checkNear(z, -58.0f * 10.0f / 60.0f, kTolerance, "gravity multiple ticks");
    checkNear(movementApplyGravityZ(-399.9f, config.gravityZ, config.maximumFallSpeed, kDt),
              -400.0f, kTolerance, "maximum fall speed clamp");
    checkNear(movementApplyGravityZ(10.0f, config.gravityZ, config.maximumFallSpeed, kDt),
              10.0f - 58.0f / 60.0f, kTolerance, "positive vertical velocity reduced");
    check(movementApplyGravityZ(0.0f, config.gravityZ, config.maximumFallSpeed, kDt) < 0.0f,
          "zero vertical velocity becomes negative");
}

void testGroundJump()
{
    const MovementConfig config = currentConfig();

    MovementState held = defaultState(false);
    MovementStepResult heldResult = simulateBasicMovementStep(
        held, commandFor(glm::vec2(0.0f), true, false), config, collisionFor(true), kDt);
    check(heldResult.events.didGroundJump, "grounded held jump emits event");
    checkNear(held.baseVelocity.z, 19.0f, kTolerance, "grounded held jump Z");
    check(!held.ground.onGround, "ground jump clears raw ground");
    checkNear(held.jump.coyoteTimerSeconds, 0.0f, kTolerance, "ground jump clears coyote");
    checkNear(held.jump.jumpIntentTimerSeconds, 0.0f, kTolerance, "ground jump clears intent");
    check(held.jump.airJumpsLeft == AIR_JUMPS_MAX, "ground jump resets air jump count");
    check(held.jump.airJumpLocked, "ground jump locks air jump");
    check(!held.jump.airJumpArmed, "ground jump disarms air jump");

    MovementState pressed = defaultState(false);
    MovementStepResult pressedResult = simulateBasicMovementStep(
        pressed, commandFor(glm::vec2(0.0f), false, true), config, collisionFor(true), kDt);
    check(pressedResult.events.didGroundJump, "grounded pressed jump emits event");

    MovementState coyote = defaultState(false);
    coyote.jump.coyoteTimerSeconds = kDt + 0.001f;
    MovementStepResult coyoteResult = simulateBasicMovementStep(
        coyote, commandFor(glm::vec2(0.0f), false, true), config, collisionFor(false), kDt);
    check(coyoteResult.events.didGroundJump, "active coyote timer allows ground jump");

    MovementState expired = defaultState(false);
    expired.jump.coyoteTimerSeconds = config.coyoteSeconds;
    expired.jump.airJumpsLeft = 0;
    expired.jump.airJumpArmed = false;
    MovementStepResult expiredResult = simulateBasicMovementStep(
        expired, commandFor(glm::vec2(0.0f), false, true), config, collisionFor(false), kDt);
    check(!expiredResult.events.didGroundJump, "0.001 coyote expires within one fixed tick");
    checkNear(expired.jump.coyoteTimerSeconds, 0.0f, kTolerance, "expired coyote timer clears");
    checkNear(expired.jump.jumpIntentTimerSeconds, config.jumpBufferSeconds, kTolerance,
              "failed jump keeps buffer active");

    MovementState buffered = defaultState(false);
    buffered.jump.airJumpsLeft = 0;
    buffered.jump.airJumpArmed = false;
    simulateBasicMovementStep(buffered, commandFor(glm::vec2(0.0f), false, true),
                              config, collisionFor(false), kDt);
    MovementStepResult bufferedResult = simulateBasicMovementStep(
        buffered, commandFor(), config, collisionFor(true), kDt);
    check(bufferedResult.events.didGroundJump, "jump buffer fires when ground returns");
}

void testHeldJump()
{
    const MovementConfig config = currentConfig();
    MovementState state = defaultState(false);

    MovementStepResult first = simulateBasicMovementStep(
        state, commandFor(glm::vec2(0.0f), true, false), config, collisionFor(true), kDt);
    check(first.events.didGroundJump, "held jump first grounded tick jumps");

    MovementStepResult second = simulateBasicMovementStep(
        state, commandFor(glm::vec2(0.0f), true, false), config, collisionFor(false), kDt);
    check(!second.events.didGroundJump && !second.events.didAirJump,
          "held jump does not repeat while airborne and unarmed");
    checkNear(state.jump.jumpIntentTimerSeconds, config.jumpBufferSeconds, kTolerance,
              "held jump refreshes intent while held");

    MovementStepResult released = simulateBasicMovementStep(
        state, commandFor(glm::vec2(0.0f), false, false), config, collisionFor(false), kDt);
    check(!released.events.didAirJump, "release does not jump");
    check(state.jump.airJumpArmed, "release arms air jump");
    check(!state.jump.airJumpLocked, "release unlocks air jump");
    checkNear(state.jump.jumpIntentTimerSeconds, 0.0f, kTolerance,
              "release clears jump intent");
}

void testAirJump()
{
    const MovementConfig config = currentConfig();
    MovementState state = defaultState(false);
    state.baseVelocity = glm::vec3(3.0f, 4.0f, -2.0f);
    state.externalImpulse = glm::vec3(9.0f, -7.0f, 5.0f);
    state.jump.airJumpsLeft = 1;
    state.jump.airJumpArmed = false;
    state.jump.airJumpLocked = true;
    state.jump.jumpHeldPreviously = true;

    MovementStepEvents releaseEvents;
    applyBasicJump(state, commandFor(glm::vec2(0.0f), false, false),
                   config, kDt, &releaseEvents);
    check(state.jump.airJumpArmed, "release arms air jump before air jump");
    check(!state.jump.airJumpLocked, "release unlocks air jump before air jump");

    MovementStepEvents jumpEvents;
    applyBasicJump(state, commandFor(glm::vec2(0.0f), true, true),
                   config, kDt, &jumpEvents);
    check(jumpEvents.didAirJump, "air jump emits event");
    checkNear(state.baseVelocity.z, 19.0f, kTolerance, "air jump sets vertical velocity");
    checkNear(state.baseVelocity.x, 3.0f, kTolerance, "air jump preserves horizontal X");
    checkNear(state.baseVelocity.y, 4.0f, kTolerance, "air jump preserves horizontal Y");
    checkVec3(state.externalImpulse, glm::vec3(9.0f, -7.0f, 5.0f),
              "air jump preserves external impulse");
    check(state.jump.airJumpsLeft == 0, "air jump consumes one charge");
    check(!state.jump.airJumpArmed, "air jump disarms itself");
    check(state.jump.airJumpLocked, "air jump locks itself");

    MovementStepEvents secondEvents;
    applyBasicJump(state, commandFor(glm::vec2(0.0f), false, false), config, kDt);
    applyBasicJump(state, commandFor(glm::vec2(0.0f), true, true),
                   config, kDt, &secondEvents);
    check(!secondEvents.didAirJump, "second air jump rejected with no charges");
}

void testCoyoteAndBufferTiming()
{
    const MovementConfig config = currentConfig();

    MovementState zero = defaultState(false);
    zero.jump.coyoteTimerSeconds = 0.0f;
    zero.jump.airJumpsLeft = 0;
    MovementStepResult zeroResult = simulateBasicMovementStep(
        zero, commandFor(glm::vec2(0.0f), false, true), config, collisionFor(false), kDt);
    check(!zeroResult.events.didGroundJump, "zero coyote rejects ground jump");

    MovementState oneTick = defaultState(false);
    oneTick.jump.coyoteTimerSeconds = kDt;
    oneTick.jump.airJumpsLeft = 0;
    MovementStepResult oneTickResult = simulateBasicMovementStep(
        oneTick, commandFor(glm::vec2(0.0f), false, true), config, collisionFor(false), kDt);
    check(!oneTickResult.events.didGroundJump, "exact one-tick coyote expires before eligibility");

    MovementState tiny = defaultState(false);
    tiny.jump.coyoteTimerSeconds = 0.001f;
    tiny.jump.airJumpsLeft = 0;
    MovementStepResult tinyResult = simulateBasicMovementStep(
        tiny, commandFor(glm::vec2(0.0f), false, true), config, collisionFor(false), kDt);
    check(!tinyResult.events.didGroundJump, "0.001 coyote boundary expires at 1/60");

    MovementState active = defaultState(false);
    active.jump.coyoteTimerSeconds = kDt + 0.0001f;
    MovementStepResult activeResult = simulateBasicMovementStep(
        active, commandFor(glm::vec2(0.0f), false, true), config, collisionFor(false), kDt);
    check(activeResult.events.didGroundJump, "more than one fixed tick coyote is active");

    MovementState buffer = defaultState(false);
    buffer.jump.jumpIntentTimerSeconds = config.jumpBufferSeconds;
    buffer.jump.airJumpsLeft = 0;
    for (int i = 0; i < 7; ++i)
        applyBasicJump(buffer, commandFor(), config, kDt);
    check(buffer.jump.jumpIntentTimerSeconds > 0.0f,
          "0.12 second buffer survives seven fixed ticks");
    applyBasicJump(buffer, commandFor(), config, kDt);
    checkNear(buffer.jump.jumpIntentTimerSeconds, 0.0f, kTolerance,
              "0.12 second buffer expires on eighth fixed tick");
}

void testExternalImpulse()
{
    const MovementConfig config = currentConfig();
    const float impulseDecay = std::exp(-config.externalImpulseDecay * kDt);

    MovementState noInput = defaultState(false);
    noInput.externalImpulse = glm::vec3(10.0f, 0.0f, 5.0f);
    simulateBasicMovementStep(noInput, commandFor(), config, collisionFor(false), kDt);
    checkVec3(noInput.externalImpulse, glm::vec3(10.0f, 0.0f, 5.0f) * impulseDecay,
              "no WASD impulse persists and decays");

    MovementState directClear = defaultState(false);
    directClear.externalImpulse = glm::vec3(10.0f, -20.0f, 5.0f);
    MovementCommand move = commandFor(glm::vec2(1.0f, 0.0f));
    applyBasicExternalImpulseControl(directClear, move);
    checkVec3(directClear.externalImpulse, glm::vec3(0.0f, 0.0f, 5.0f),
              "WASD clears horizontal external impulse and preserves positive Z before friction");

    MovementState wasd = defaultState(false);
    wasd.externalImpulse = glm::vec3(10.0f, -20.0f, 5.0f);
    simulateBasicMovementStep(wasd, move, config, collisionFor(false), kDt);
    checkVec3(wasd.externalImpulse, glm::vec3(0.0f, 0.0f, 5.0f) * impulseDecay,
              "WASD clear then friction decays preserved Z");

    glm::vec3 clamped(300.0f, 0.0f, 0.0f);
    movementDecayAndClampExternalImpulse(clamped, config, 1.0f, kDt);
    checkNear(glm::length(glm::vec2(clamped.x, clamped.y)),
              config.maximumExternalImpulseSpeed, kTolerance,
              "horizontal impulse clamp");

    glm::vec2 groundBase(20.0f, 0.0f);
    glm::vec2 airBase(20.0f, 0.0f);
    const glm::vec2 groundAfter = movementApplyBaseFrictionXY(
        groundBase, true, false, 1.0f, 1.0f, config, kDt);
    const glm::vec2 airAfter = movementApplyBaseFrictionXY(
        airBase, false, false, 1.0f, 1.0f, config, kDt);
    check(glm::length(groundAfter) < glm::length(airAfter),
          "ground base friction decays faster than air friction");

    const glm::vec2 sizeScaled = movementApplyBaseFrictionXY(
        groundBase, true, false, 4.0f, 1.0f, config, kDt);
    const float expectedSizeScaled = 20.0f *
        std::exp(-(config.groundFrictionAmount *
                   movementSizeScaleFactor(4.0f, config.frictionSizeExponent)) * kDt);
    checkNear(sizeScaled.x, expectedSizeScaled, kTolerance, "size-scaled friction decay");

    glm::vec3 repeated(30.0f, 0.0f, 0.0f);
    for (int i = 0; i < 3; ++i)
        movementDecayAndClampExternalImpulse(repeated, config, 1.0f, kDt);
    checkNear(repeated.x, 30.0f * impulseDecay * impulseDecay * impulseDecay,
              kTolerance, "external impulse multiple tick decay");
}

void testEventReset()
{
    const MovementConfig config = currentConfig();
    MovementState state = defaultState(false);
    MovementStepResult jump = simulateBasicMovementStep(
        state, commandFor(glm::vec2(0.0f), true, false), config, collisionFor(true), kDt);
    check(jump.events.didGroundJump, "event reset setup emits ground jump");

    MovementStepResult next = simulateBasicMovementStep(
        state, commandFor(), config, collisionFor(false), kDt);
    check(!next.events.didGroundJump && !next.events.didAirJump && !next.events.didLand,
          "one-tick events reset on later step");
}

void testFiniteValidation()
{
    MovementCommand command = commandFor();
    check(movementIsFinite(command), "finite command accepted");
    command.moveAxes.x = std::numeric_limits<float>::quiet_NaN();
    check(!movementIsFinite(command), "NaN command axes rejected");

    MovementState velocity = defaultState();
    velocity.baseVelocity.y = std::numeric_limits<float>::infinity();
    check(!movementIsFinite(velocity), "infinite velocity rejected");

    MovementState position = defaultState();
    position.position.z = std::numeric_limits<float>::quiet_NaN();
    check(!movementIsFinite(position), "NaN position rejected");

    MovementState impulse = defaultState();
    impulse.externalImpulse.x = std::numeric_limits<float>::infinity();
    check(!movementIsFinite(impulse), "infinite external impulse rejected");
}

void testConversionParity()
{
    MovementState grounded = defaultState(true);
    grounded.baseVelocity = glm::vec3(7.0f, -3.0f, 0.0f);
    grounded.externalImpulse = glm::vec3(12.0f, 2.0f, 4.0f);
    grounded.ground.airborneTimerSeconds = 0.2f;
    runSharedVsLegacyCase(grounded,
                          commandFor(glm::vec2(1.0f, 1.0f), true, false),
                          collisionFor(true),
                          "conversion parity grounded held jump");

    MovementState airborne = defaultState(false);
    airborne.baseVelocity = glm::vec3(-10.0f, 8.0f, -5.0f);
    airborne.externalImpulse = glm::vec3(30.0f, -45.0f, 8.0f);
    airborne.jump.airJumpsLeft = 1;
    airborne.jump.airJumpArmed = true;
    runSharedVsLegacyCase(airborne,
                          commandFor(glm::vec2(0.0f), true, true),
                          collisionFor(false),
                          "conversion parity air jump");
}

float randomFloat(std::mt19937& rng, float lo, float hi)
{
    std::uniform_real_distribution<float> dist(lo, hi);
    return dist(rng);
}

bool randomBool(std::mt19937& rng)
{
    std::uniform_int_distribution<int> dist(0, 1);
    return dist(rng) != 0;
}

MovementState randomState(std::mt19937& rng, int index)
{
    MovementState state = defaultState(randomBool(rng));
    state.lifecycle = MovementLifecycleIdentity{static_cast<uint32_t>(100 + index), 200};
    state.position = glm::vec3(randomFloat(rng, -20.0f, 20.0f),
                               randomFloat(rng, -20.0f, 20.0f),
                               randomFloat(rng, -5.0f, 25.0f));
    state.baseVelocity = glm::vec3(randomFloat(rng, -80.0f, 80.0f),
                                   randomFloat(rng, -80.0f, 80.0f),
                                   randomFloat(rng, -120.0f, 80.0f));
    state.externalImpulse = glm::vec3(randomFloat(rng, -180.0f, 180.0f),
                                      randomFloat(rng, -180.0f, 180.0f),
                                      randomFloat(rng, -80.0f, 80.0f));
    state.lastInputMoveAxes = movementClampUnitOrZero(
        glm::vec2(randomFloat(rng, -1.0f, 1.0f), randomFloat(rng, -1.0f, 1.0f)));
    state.sizeScale = randomFloat(rng, 0.2f, 4.0f);
    state.ground.stableOnGround = randomBool(rng);
    state.ground.wasOnGround = randomBool(rng);
    state.ground.hasWorldContact = randomBool(rng);
    state.ground.realWorldContactThisFrame = randomBool(rng);
    state.ground.didLand = randomBool(rng);
    state.ground.groundLostTimerSeconds = randomFloat(rng, 0.0f, 0.15f);
    state.ground.airborneTimerSeconds = randomFloat(rng, 0.0f, 0.3f);
    state.ground.landingCooldownSeconds = randomFloat(rng, 0.0f, 0.4f);
    state.jump.airJumpsLeft = randomBool(rng) ? 1 : 0;
    state.jump.jumpHeldPreviously = randomBool(rng);
    state.jump.airJumpLocked = randomBool(rng);
    state.jump.airJumpArmed = randomBool(rng);
    state.jump.jumpIntentTimerSeconds = randomFloat(rng, 0.0f, 0.14f);
    state.jump.coyoteTimerSeconds = randomFloat(rng, 0.0f, 0.04f);
    state.jump.didGroundJump = randomBool(rng);
    state.jump.didAirJump = randomBool(rng);
    state.dash.dashAvailable = randomBool(rng);
    state.dash.dashMovementTicks = static_cast<int>(randomFloat(rng, 0.0f, 98.0f));
    state.dash.frictionOverride = randomFloat(rng, 0.0f, 1.2f);
    state.dash.tickPerfectDash = randomBool(rng);
    state.downDash.available = randomBool(rng);
    state.freeze.available = randomBool(rng);
    state.groundReturn.available = randomBool(rng);
    return state;
}

MovementCommand randomCommand(std::mt19937& rng)
{
    MovementCommand command;
    command.moveAxes = movementClampUnitOrZero(
        glm::vec2(randomFloat(rng, -1.0f, 1.0f), randomFloat(rng, -1.0f, 1.0f)));
    command.jumpHeld = randomBool(rng);
    command.jumpPressed = randomBool(rng);
    command.dashPressed = randomBool(rng);
    command.downDashPressed = randomBool(rng);
    command.freezeHeld = randomBool(rng);
    command.movementDirectionPressed = movementHasMoveInput(command.moveAxes);
    command.movementDirectionFreshPressed = randomBool(rng);
    command.movementDirectionReleased = randomBool(rng);
    command.movementDirectionChanged = randomBool(rng);
    command.movementHeldDurationSeconds = randomFloat(rng, 0.0f, 1.0f);
    return command;
}

MovementCollisionFeedback randomCollision(std::mt19937& rng)
{
    MovementCollisionFeedback collision;
    collision.onGround = randomBool(rng);
    collision.hasWorldContact = randomBool(rng);
    collision.realWorldContactThisFrame = randomBool(rng);
    return collision;
}

void testRandomizedParity()
{
    std::mt19937 rng(kRandomSeed);
    const int failuresBefore = gFailures;
    for (int i = 0; i < 256; ++i) {
        const MovementState state = randomState(rng, i);
        const MovementCommand command = randomCommand(rng);
        const MovementCollisionFeedback collision = randomCollision(rng);
        std::ostringstream context;
        context << "random parity seed=" << kRandomSeed << " case=" << i
                << " move=(" << command.moveAxes.x << "," << command.moveAxes.y << ")"
                << " jumpHeld=" << command.jumpHeld
                << " jumpPressed=" << command.jumpPressed
                << " collisionGround=" << collision.onGround;
        runSharedVsLegacyCase(state, command, collision, context.str());
    }
    check(gFailures == failuresBefore,
          "randomized parity had no mismatches for seed 0x2A2026 across 256 cases");
}

void testPhaseOrder()
{
    const MovementConfig config = currentConfig();

    MovementState gravity = defaultState(false);
    gravity.baseVelocity.z = 0.0f;
    applyPreCollisionBasicMovement(gravity, commandFor(), config, kDt);
    checkNear(gravity.baseVelocity.z, -58.0f / 60.0f, kTolerance,
              "gravity applies before collision feedback");

    MovementState jumpAfterCollision = defaultState(false);
    MovementStepResult jumpResult = simulateBasicMovementStep(
        jumpAfterCollision,
        commandFor(glm::vec2(0.0f), false, true),
        config,
        collisionFor(true),
        kDt);
    check(jumpResult.events.didGroundJump, "jump uses post-collision ground feedback");
    checkNear(jumpAfterCollision.baseVelocity.z, 19.0f, kTolerance,
              "jump overwrites post-collision vertical velocity");

    MovementConfig custom = config;
    custom.groundSpeed = 33.0f;
    custom.airSpeed = 7.0f;
    MovementState walk = defaultState(false);
    simulateBasicMovementStep(walk, commandFor(glm::vec2(1.0f, 0.0f)),
                              custom, collisionFor(true), kDt);
    checkNear(walk.baseVelocity.x, 33.0f, kTolerance,
              "walk uses collision feedback ground state");

    MovementState friction = defaultState(false);
    friction.baseVelocity = glm::vec3(20.0f, 0.0f, 0.0f);
    simulateBasicMovementStep(friction, commandFor(glm::vec2(0.0f), true, false),
                              config, collisionFor(true), kDt);
    checkNear(friction.baseVelocity.z, 19.0f, kTolerance,
              "friction runs after jump without modifying jump Z");
}

} // namespace

int main()
{
    testWalking();
    testGravity();
    testGroundJump();
    testHeldJump();
    testAirJump();
    testCoyoteAndBufferTiming();
    testExternalImpulse();
    testEventReset();
    testFiniteValidation();
    testConversionParity();
    testRandomizedParity();
    testPhaseOrder();

    if (gFailures != 0) {
        std::cerr << "[movement-basic-kernel-test] FAIL failures=" << gFailures << "\n";
        return 1;
    }

    std::cout << "[movement-basic-kernel-test] PASS seed=" << kRandomSeed
              << " randomizedCases=256 maxDifference<=" << kTolerance << "\n";
    return 0;
}
