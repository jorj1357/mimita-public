// 07 21 2026, 16 30
/* purpose
* Verifies the shared Stage 2B dash, down-dash, freeze, and protection kernel.
* Exercises target special-movement formulas, events, conversion, phase order, and invariants.
* Keeps all checks deterministic and headless for local, server, replay, and NPC reuse later.
* Does NOT launch mimita.exe, render, play audio, send packets, or open sockets.
* Does NOT test collision sweeps, universal contact reset generation, or networking authority.
* Does NOT duplicate projectile, weapon, prediction, reconciliation, or replay export behavior.
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
#include "physics/movement/movement-conversion.h"

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

constexpr uint32_t kRandomSeed = 0x2B2026u;
constexpr int kRandomCases = 512;
constexpr float kDt = 1.0f / 60.0f;
constexpr float kTolerance = 0.0003f;

int gFailures = 0;
float gMaxDifference = 0.0f;

void trackDiff(float actual, float expected)
{
    gMaxDifference = std::max(gMaxDifference, std::fabs(actual - expected));
}

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
    trackDiff(actual, expected);
    if (std::fabs(actual - expected) > tolerance) {
        std::ostringstream out;
        out << message << " expected=" << expected << " actual=" << actual
            << " diff=" << (actual - expected);
        fail(out.str());
    }
}

void checkVec2(glm::vec2 actual,
               glm::vec2 expected,
               const std::string& message,
               float tolerance = kTolerance)
{
    checkNear(actual.x, expected.x, tolerance, message + ".x");
    checkNear(actual.y, expected.y, tolerance, message + ".y");
}

void checkVec3(glm::vec3 actual,
               glm::vec3 expected,
               const std::string& message,
               float tolerance = kTolerance)
{
    checkNear(actual.x, expected.x, tolerance, message + ".x");
    checkNear(actual.y, expected.y, tolerance, message + ".y");
    checkNear(actual.z, expected.z, tolerance, message + ".z");
}

float lengthXY(glm::vec3 value)
{
    return glm::length(glm::vec2(value.x, value.y));
}

MovementConfig currentConfig()
{
    return makeCurrentRuntimeMovementConfig();
}

MovementState defaultState(bool onGround = false)
{
    MovementState state;
    state.lifecycle = MovementLifecycleIdentity{100, 200};
    state.sizeScale = 1.0f;
    state.ground.onGround = onGround;
    state.ground.stableOnGround = onGround;
    state.ground.wasOnGround = onGround;
    state.ground.hasWorldContact = onGround;
    state.jump.airJumpsLeft = 1;
    state.jump.airJumpArmed = true;
    state.dash.dashAvailable = true;
    state.dash.frictionOverride = 1.0f;
    state.downDash.available = true;
    state.freeze.available = true;
    return state;
}

MovementCommand commandFor(glm::vec2 axes = glm::vec2(0.0f))
{
    MovementCommand command;
    command.lifecycle = MovementLifecycleIdentity{100, 200};
    command.moveAxes = movementClampUnitOrZero(axes);
    command.horizontalCameraForward = glm::vec3(1.0f, 0.0f, 0.0f);
    command.lookYaw = 0.0f;
    command.movementDirectionPressed = movementHasMoveInput(command.moveAxes);
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

void expectDirection(const MovementCommand& command,
                     glm::vec2 expected,
                     const std::string& label)
{
    checkVec2(movementDashDirection(command),
              movementNormalizeDirectionOrZero(expected),
              label,
              0.0001f);
}

void testDashDirection()
{
    MovementCommand command = commandFor();
    command.horizontalCameraForward = glm::vec3(0.0f, 10.0f, 100.0f);
    expectDirection(command, glm::vec2(0.0f, 1.0f),
                    "no WASD uses horizontal camera forward and ignores pitch");

    expectDirection(commandFor(glm::vec2(0.0f, 1.0f)), glm::vec2(0.0f, 1.0f), "W");
    expectDirection(commandFor(glm::vec2(0.0f, -1.0f)), glm::vec2(0.0f, -1.0f), "S");
    expectDirection(commandFor(glm::vec2(-1.0f, 0.0f)), glm::vec2(-1.0f, 0.0f), "A");
    expectDirection(commandFor(glm::vec2(1.0f, 0.0f)), glm::vec2(1.0f, 0.0f), "D");
    expectDirection(commandFor(glm::vec2(-1.0f, 1.0f)), glm::vec2(-1.0f, 1.0f), "W+A");
    expectDirection(commandFor(glm::vec2(1.0f, 1.0f)), glm::vec2(1.0f, 1.0f), "W+D");
    expectDirection(commandFor(glm::vec2(-1.0f, -1.0f)), glm::vec2(-1.0f, -1.0f), "S+A");
    expectDirection(commandFor(glm::vec2(1.0f, -1.0f)), glm::vec2(1.0f, -1.0f), "S+D");
    expectDirection(commandFor(glm::vec2(9.0f, 0.0f)), glm::vec2(1.0f, 0.0f),
                    "non-normalized input");

    command = commandFor();
    command.horizontalCameraForward = glm::vec3(0.0f, 0.0f, 5.0f);
    command.lookYaw = 90.0f;
    expectDirection(command, glm::vec2(0.0f, 1.0f),
                    "zero horizontal camera forward falls back to yaw");

    command.horizontalCameraForward.x = std::numeric_limits<float>::quiet_NaN();
    command.lookYaw = 180.0f;
    expectDirection(command, glm::vec2(-1.0f, 0.0f),
                    "invalid camera forward falls back to yaw");
}

void expectDashActivation(bool onGround, const std::string& label)
{
    const MovementConfig config = currentConfig();
    MovementState state = defaultState(onGround);
    state.baseVelocity = glm::vec3(10.0f, -5.0f, 7.0f);
    state.externalImpulse = glm::vec3(3.0f, 4.0f, 5.0f);

    MovementCommand command = commandFor(glm::vec2(1.0f, 0.0f));
    command.dashPressed = true;

    MovementStepEvents events;
    const bool activated = tryActivateDash(state, command, config, events);
    check(activated, label + " activates");
    check(events.didDash, label + " emits dash event");
    checkNear(state.baseVelocity.x, 60.0f, kTolerance, label + " adds 50 X");
    checkNear(state.baseVelocity.y, -5.0f, kTolerance, label + " preserves Y additively");
    checkNear(state.baseVelocity.z, 7.0f, kTolerance, label + " preserves Z");
    checkVec3(state.externalImpulse, glm::vec3(3.0f, 4.0f, 5.0f),
              label + " preserves external impulse");
    check(!state.dash.dashAvailable, label + " consumes availability");
    check(state.jump.airJumpsLeft == 0, label + " consumes air jump count");
    check(state.dashMomentumProtection.active, label + " starts momentum protection");
}

void testDashForce()
{
    expectDashActivation(true, "ground dash");
    expectDashActivation(false, "air dash");
    checkNear(movementDashImpulse(currentConfig()), 50.0f, kTolerance,
              "shared dash impulse is approved 50");
}

void testDashEdgeAvailability()
{
    const MovementConfig config = currentConfig();
    MovementState state = defaultState(false);
    MovementCommand command = commandFor(glm::vec2(1.0f, 0.0f));
    command.dashPressed = true;

    MovementStepEvents events;
    check(tryActivateDash(state, command, config, events),
          "fresh Shift and availability dash");
    const glm::vec3 afterFirstVelocity = state.baseVelocity;
    const glm::vec3 afterFirstImpulse = state.externalImpulse;

    command.dashPressed = false;
    events = {};
    check(!tryActivateDash(state, command, config, events),
          "held Shift without fresh edge does not repeat");
    check(!events.didDash, "held Shift emits no dash event");
    checkVec3(state.baseVelocity, afterFirstVelocity,
              "held Shift does not mutate velocity");

    command.dashPressed = true;
    events = {};
    check(!tryActivateDash(state, command, config, events),
          "fresh Shift while unavailable is rejected");
    check(!events.didDash, "unavailable dash emits no success event");
    checkVec3(state.baseVelocity, afterFirstVelocity,
              "unavailable dash does not mutate velocity");
    checkVec3(state.externalImpulse, afterFirstImpulse,
              "unavailable dash preserves external impulse");
    check(!state.dash.dashAvailable,
          "unavailable dash does not consume a new availability state");

    restoreSpecialAbilityAvailability(state);
    command.dashPressed = false;
    events = {};
    check(!tryActivateDash(state, command, config, events),
          "restored availability plus still-held Shift does not auto dash");
    check(state.dash.dashAvailable,
          "still-held Shift leaves restored dash availability");

    command.dashPressed = true;
    events = {};
    check(tryActivateDash(state, command, config, events),
          "fresh press after restore dashes");
}

void testDashMomentumProtection()
{
    const MovementConfig config = currentConfig();

    MovementState sequence = defaultState(false);
    MovementCommand diagonal = commandFor(glm::vec2(-1.0f, 1.0f));
    diagonal.dashPressed = true;
    diagonal.movementDirectionFreshPressed = true;
    simulateMovementStepWithSpecials(sequence, diagonal, config, collisionFor(false), kDt);
    const float speedAfterDash = lengthXY(sequence.baseVelocity);
    check(sequence.dashMomentumProtection.active, "W+A dash starts protection");
    check(speedAfterDash > 60.0f, "W+A dash creates more than walk speed");

    diagonal.dashPressed = false;
    diagonal.movementDirectionFreshPressed = false;
    simulateMovementStepWithSpecials(sequence, diagonal, config, collisionFor(false), kDt);
    check(sequence.dashMomentumProtection.active, "same W+A held keeps protection");
    check(lengthXY(sequence.baseVelocity) > 50.0f,
          "same W+A held does not overwrite dash with walking");

    MovementCommand released = commandFor();
    released.movementDirectionReleased = true;
    simulateMovementStepWithSpecials(sequence, released, config, collisionFor(false), kDt);
    check(sequence.dashMomentumProtection.active, "release keeps dash protection");
    check(lengthXY(sequence.baseVelocity) > 45.0f,
          "release does not erase dash momentum");

    MovementCommand right = commandFor(glm::vec2(1.0f, 0.0f));
    right.movementDirectionFreshPressed = true;
    simulateMovementStepWithSpecials(sequence, right, config, collisionFor(false), kDt);
    check(!sequence.dashMomentumProtection.active,
          "later fresh D press ends protection");
    checkNear(sequence.baseVelocity.x, 20.0f, 0.001f,
              "later D overwrites X with walking velocity before friction");
    checkNear(sequence.baseVelocity.y, 0.0f, 0.001f,
              "later D overwrites Y with walking velocity before friction");

    MovementState wDash = defaultState(false);
    MovementCommand w = commandFor(glm::vec2(0.0f, 1.0f));
    w.dashPressed = true;
    w.movementDirectionFreshPressed = true;
    simulateMovementStepWithSpecials(wDash, w, config, collisionFor(false), kDt);
    w.dashPressed = false;
    w.movementDirectionFreshPressed = false;
    simulateMovementStepWithSpecials(wDash, w, config, collisionFor(false), kDt);
    check(wDash.dashMomentumProtection.active,
          "W dash then W held keeps protection");
    check(lengthXY(wDash.baseVelocity) > 50.0f,
          "W held after dash preserves momentum");

    MovementCommand wPlusA = commandFor(glm::vec2(-1.0f, 1.0f));
    wPlusA.movementDirectionChanged = true;
    simulateMovementStepWithSpecials(wDash, wPlusA, config, collisionFor(false), kDt);
    check(!wDash.dashMomentumProtection.active,
          "W dash then W+A change ends protection");
    checkNear(lengthXY(wDash.baseVelocity), 20.0f, 0.001f,
              "changed movement direction takes walking control");

    MovementState fallback = defaultState(false);
    MovementCommand noInput = commandFor();
    noInput.horizontalCameraForward = glm::vec3(0.0f, 1.0f, 10.0f);
    noInput.dashPressed = true;
    simulateMovementStepWithSpecials(fallback, noInput, config, collisionFor(false), kDt);
    check(fallback.dashMomentumProtection.active,
          "no-input dash starts fallback protection");
    check(fallback.dashMomentumProtection.usedCameraForwardFallback,
          "no-input dash records camera-forward fallback");
    const float fallbackSpeed = lengthXY(fallback.baseVelocity);
    noInput.dashPressed = false;
    simulateMovementStepWithSpecials(fallback, noInput, config, collisionFor(false), kDt);
    check(lengthXY(fallback.baseVelocity) < fallbackSpeed &&
              lengthXY(fallback.baseVelocity) > 30.0f,
          "no-input after fallback dash keeps momentum with friction only");
    MovementCommand laterW = commandFor(glm::vec2(0.0f, 1.0f));
    laterW.movementDirectionFreshPressed = true;
    simulateMovementStepWithSpecials(fallback, laterW, config, collisionFor(false), kDt);
    check(!fallback.dashMomentumProtection.active,
          "later W after fallback dash ends protection");
    checkNear(lengthXY(fallback.baseVelocity), 20.0f, 0.001f,
              "later W after fallback dash takes walking control");

    resetSpecialMovementLifecycleState(fallback);
    check(!fallback.dashMomentumProtection.active,
          "lifecycle reset clears dash momentum protection");
}

void testDownDash()
{
    const MovementConfig config = currentConfig();

    for (bool grounded : {false, true}) {
        MovementState state = defaultState(grounded);
        state.baseVelocity = glm::vec3(8.0f, -9.0f, grounded ? 45.0f : -12.0f);
        state.externalImpulse = glm::vec3(1.0f, 2.0f, 3.0f);
        MovementCommand command = commandFor();
        command.downDashPressed = true;
        MovementStepEvents events;
        check(tryActivateDownDash(state, command, config, events),
              grounded ? "grounded Q down dash succeeds" : "airborne Q down dash succeeds");
        checkNear(state.baseVelocity.z, -100.0f, kTolerance,
                  grounded ? "grounded Q sets Z" : "airborne Q sets Z");
        checkNear(state.baseVelocity.x, 8.0f, kTolerance, "down dash preserves X");
        checkNear(state.baseVelocity.y, -9.0f, kTolerance, "down dash preserves Y");
        checkVec3(state.externalImpulse, glm::vec3(1.0f, 2.0f, 3.0f),
                  "down dash preserves all external impulse");
        check(events.didDownDash, "down dash emits event");
    }

    MovementState unavailable = defaultState(false);
    unavailable.downDash.available = false;
    unavailable.baseVelocity = glm::vec3(4.0f, 5.0f, 6.0f);
    unavailable.externalImpulse = glm::vec3(7.0f, 8.0f, 9.0f);
    MovementCommand command = commandFor();
    command.downDashPressed = true;
    MovementStepEvents events;
    check(!tryActivateDownDash(unavailable, command, config, events),
          "unavailable down dash rejected");
    checkVec3(unavailable.baseVelocity, glm::vec3(4.0f, 5.0f, 6.0f),
              "unavailable down dash does not mutate velocity");
    checkVec3(unavailable.externalImpulse, glm::vec3(7.0f, 8.0f, 9.0f),
              "unavailable down dash preserves impulse");
    check(!events.didDownDash, "unavailable down dash emits no success");

    restoreSpecialAbilityAvailability(unavailable);
    command.downDashPressed = false;
    check(!tryActivateDownDash(unavailable, command, config, events),
          "restored availability plus held Q without fresh edge does not repeat");
    command.downDashPressed = true;
    events = {};
    check(tryActivateDownDash(unavailable, command, config, events),
          "fresh Q after restore succeeds");

    MovementState phase = defaultState(true);
    phase.baseVelocity.z = 32.0f;
    MovementStepEvents phaseEvents;
    applyPreCollisionBasicMovement(phase, commandFor(), config, kDt);
    MovementCommand down = commandFor();
    down.downDashPressed = true;
    applySpecialMovementPreCollision(phase, down, config, kDt, phaseEvents);
    checkNear(phase.baseVelocity.z, -100.0f, kTolerance,
              "down dash Z is available to collision in pre-collision phase");
}

void testFreezeActivationCurveAndVertical()
{
    const MovementConfig config = currentConfig();
    MovementState state = defaultState(false);
    state.baseVelocity = glm::vec3(10.0f, -20.0f, 30.0f);
    state.externalImpulse = glm::vec3(40.0f, 50.0f, 60.0f);

    MovementCommand freeze = commandFor();
    freeze.freezeHeld = true;
    freeze.freezePressed = true;
    MovementStepEvents events;
    updateFreeze(state, freeze, config, kDt, events);
    checkVec3(state.baseVelocity, glm::vec3(0.0f, 0.0f, 30.0f),
              "freeze activation zeros base X/Y and preserves Z");
    checkVec3(state.externalImpulse, glm::vec3(40.0f, 50.0f, 60.0f),
              "freeze activation preserves all external impulse");
    check(state.freeze.active && !state.freeze.available,
          "freeze activation sets active and consumes availability");
    checkNear(state.freeze.timerSeconds, 0.0f, kTolerance,
              "freeze activation starts at timer zero");
    check(events.didFreeze && events.freezeStarted,
          "freeze activation emits one start event");

    events = {};
    updateFreeze(state, freeze, config, kDt, events);
    check(!events.freezeStarted && !events.didFreeze,
          "continuing freeze does not repeat start event");
    checkNear(state.freeze.timerSeconds, kDt, kTolerance,
              "continuing freeze advances timer by fixed dt");

    freeze.freezeHeld = false;
    freeze.freezePressed = false;
    events = {};
    updateFreeze(state, freeze, config, kDt, events);
    check(!state.freeze.active && !state.freeze.available,
          "freeze release ends active state without restoring availability");
    check(events.freezeEnded, "freeze release emits end event");

    checkNear(movementFreezeHorizontalPassThrough(0.0f), 0.0f, 0.000001f, "freeze curve 0s");
    checkNear(movementFreezeHorizontalPassThrough(1.0f), 0.0016f, 0.000001f, "freeze curve 1s");
    checkNear(movementFreezeHorizontalPassThrough(2.0f), 0.0256f, 0.000001f, "freeze curve 2s");
    checkNear(movementFreezeHorizontalPassThrough(3.0f), 0.1296f, 0.000001f, "freeze curve 3s");
    checkNear(movementFreezeHorizontalPassThrough(4.0f), 0.4096f, 0.000001f, "freeze curve 4s");
    checkNear(movementFreezeHorizontalPassThrough(5.0f), 1.0f, 0.000001f, "freeze curve 5s");
    checkNear(movementFreezeHorizontalPassThrough(6.0f), 1.0f, 0.000001f, "freeze curve over duration");
    checkNear(movementFreezeHorizontalPassThrough(-1.0f), 0.0f, 0.000001f, "freeze curve negative");

    MovementState viewState = defaultState(false);
    viewState.baseVelocity = glm::vec3(10.0f, 20.0f, 30.0f);
    viewState.externalImpulse = glm::vec3(40.0f, 60.0f, 70.0f);
    viewState.freeze.active = true;
    viewState.freeze.timerSeconds = 2.0f;
    const MovementVelocityView view = movementVelocityViewForCollision(viewState, config);
    checkNear(view.horizontalPassThrough, 0.0256f, 0.000001f,
              "freeze velocity view uses pow4 pass-through");
    checkVec3(view.effectiveBaseVelocity, glm::vec3(0.256f, 0.512f, 30.0f),
              "freeze suppresses base X/Y but not Z", 0.00001f);
    checkVec3(view.effectiveExternalImpulse, glm::vec3(1.024f, 1.536f, 70.0f),
              "freeze suppresses impulse X/Y but not Z", 0.00001f);
    checkVec3(calculateEffectiveMovementVelocity(viewState, config),
              glm::vec3(1.28f, 2.048f, 100.0f),
              "freeze effective velocity preserves vertical movement", 0.00001f);

    MovementState gravity = defaultState(false);
    gravity.freeze.active = true;
    applyPreCollisionBasicMovement(gravity, commandFor(), config, kDt);
    checkNear(gravity.baseVelocity.z, config.gravityZ * kDt, kTolerance,
              "freeze does not suppress gravity Z");

    MovementState jump = defaultState(false);
    jump.freeze.active = true;
    jump.ground.onGround = true;
    MovementCommand jumpCommand = commandFor();
    jumpCommand.jumpPressed = true;
    simulateMovementStepWithSpecials(jump, jumpCommand, config, collisionFor(true), kDt);
    checkNear(jump.baseVelocity.z, config.jumpVerticalSpeed, kTolerance,
              "freeze does not suppress jump Z");

    MovementState down = defaultState(false);
    down.freeze.active = true;
    MovementCommand downCommand = commandFor();
    downCommand.downDashPressed = true;
    applyPreCollisionBasicMovement(down, commandFor(), config, kDt);
    applySpecialMovementPreCollision(down, downCommand, config, kDt, events);
    checkNear(down.baseVelocity.z, -100.0f, kTolerance,
              "freeze does not suppress down-dash Z");
}

void testFreezeStorageAndRecharge()
{
    MovementConfig config = currentConfig();
    config.maximumExternalImpulseSpeed = 2000.0f;

    MovementState storage = defaultState(false);
    storage.freeze.active = true;
    storage.freeze.available = false;
    storage.freeze.heldPreviously = true;
    storage.freeze.timerSeconds = 0.0f;
    storage.externalImpulse = glm::vec3(1000.0f, 0.0f, 25.0f);

    MovementCommand held = commandFor();
    held.freezeHeld = true;
    float expectedStoredX = 1000.0f;
    float effectiveAtOneSecond = 0.0f;
    float effectiveAtTwoSeconds = 0.0f;
    for (int i = 1; i <= 120; ++i) {
        simulateMovementStepWithSpecials(storage, held, config, collisionFor(false), kDt);
        expectedStoredX *= std::exp(-config.externalImpulseDecay * kDt);
        checkNear(storage.externalImpulse.x, expectedStoredX, 0.05f,
                  "freeze storage only decays external impulse through normal impulse decay");
        if (i == 60)
            effectiveAtOneSecond =
                std::fabs(movementVelocityViewForCollision(storage, config).effectiveExternalImpulse.x);
        if (i == 120)
            effectiveAtTwoSeconds =
                std::fabs(movementVelocityViewForCollision(storage, config).effectiveExternalImpulse.x);
    }
    check(effectiveAtTwoSeconds > effectiveAtOneSecond,
          "freeze pass-through weakens so stored impulse contributes more movement");
    check(storage.externalImpulse.z > 0.0f,
          "freeze storage preserves vertical external impulse decay path");

    MovementState recharge = defaultState(false);
    MovementCommand press = commandFor();
    press.freezeHeld = true;
    press.freezePressed = true;
    MovementStepEvents events;
    updateFreeze(recharge, press, config, kDt, events);
    press.freezePressed = false;
    for (int i = 0; i < 300; ++i)
        updateFreeze(recharge, press, config, kDt, events);
    checkNear(freezeHorizontalPassThrough(recharge.freeze, config), 1.0f, 0.0001f,
              "held freeze reaches zero suppression at five seconds");
    restoreSpecialAbilityAvailability(recharge);
    const float timerBeforeHeldResetTick = recharge.freeze.timerSeconds;
    events = {};
    updateFreeze(recharge, press, config, kDt, events);
    check(!events.freezeStarted &&
              recharge.freeze.timerSeconds >= timerBeforeHeldResetTick &&
              recharge.freeze.timerSeconds > 1.0f,
          "reset while E is still held does not auto-restart freeze");
    check(recharge.freeze.active && recharge.freeze.available,
          "held-through-reset freeze remains active and available for a future fresh press");

    MovementCommand release = commandFor();
    release.freezeHeld = false;
    events = {};
    updateFreeze(recharge, release, config, kDt, events);
    check(events.freezeEnded && !recharge.freeze.active && recharge.freeze.available,
          "release after reset ends active freeze and keeps restored availability");

    press.freezePressed = true;
    events = {};
    updateFreeze(recharge, press, config, kDt, events);
    check(events.freezeStarted && recharge.freeze.active && !recharge.freeze.available,
          "fresh E press after reset starts new full-strength freeze");
    checkNear(recharge.freeze.timerSeconds, 0.0f, kTolerance,
              "fresh E press after reset resets timer");

    MovementState noReset = defaultState(false);
    updateFreeze(noReset, press, config, kDt, events);
    updateFreeze(noReset, release, config, kDt, events);
    events = {};
    updateFreeze(noReset, press, config, kDt, events);
    check(!events.freezeStarted && !noReset.freeze.active,
          "re-press without reset fails to start freeze");

    restoreSpecialAbilityAvailability(noReset);
    press.freezePressed = true;
    press.freezeHeld = true;
    events = {};
    updateFreeze(noReset, press, config, kDt, events);
    check(events.freezeStarted && noReset.freeze.active,
          "reset while not held allows fresh E press");
}

void testFreezeAndDashInteraction()
{
    const MovementConfig config = currentConfig();
    MovementState fullFreeze = defaultState(false);
    fullFreeze.freeze.active = true;
    fullFreeze.freeze.timerSeconds = 0.0f;
    fullFreeze.baseVelocity = glm::vec3(5.0f, 6.0f, 7.0f);
    fullFreeze.externalImpulse = glm::vec3(8.0f, 9.0f, 10.0f);
    MovementCommand dash = commandFor(glm::vec2(1.0f, 0.0f));
    dash.dashPressed = true;
    MovementStepEvents events;
    check(!tryActivateDash(fullFreeze, dash, config, events),
          "full-strength freeze rejects dash");
    check(events.dashRejectedByFreeze, "full-strength freeze emits dash rejection");
    check(fullFreeze.dash.dashAvailable,
          "full-strength freeze does not consume dash availability");
    checkVec3(fullFreeze.baseVelocity, glm::vec3(5.0f, 6.0f, 7.0f),
              "rejected freeze dash preserves base velocity");
    checkVec3(fullFreeze.externalImpulse, glm::vec3(8.0f, 9.0f, 10.0f),
              "rejected freeze dash preserves external impulse");

    MovementState weakFreeze = defaultState(false);
    weakFreeze.freeze.active = true;
    weakFreeze.freeze.timerSeconds = 1.0f;
    events = {};
    check(tryActivateDash(weakFreeze, dash, config, events),
          "weak freeze above threshold permits dash");
    check(events.didDash, "weak freeze dash emits success");
    checkNear(weakFreeze.baseVelocity.x, 50.0f, kTolerance,
              "weak freeze dash stores full unsuppressed dash velocity");
    const MovementVelocityView view = movementVelocityViewForCollision(weakFreeze, config);
    checkNear(view.effectiveBaseVelocity.x, 50.0f * 0.0016f, 0.0001f,
              "weak freeze dash output is subject to pass-through");
}

void testEventsAndPhaseOrder()
{
    const MovementConfig config = currentConfig();

    MovementState dash = defaultState(false);
    MovementCommand dashCommand = commandFor();
    dashCommand.dashPressed = true;
    MovementStepResult dashResult =
        simulateMovementStepWithSpecials(dash, dashCommand, config, collisionFor(false), kDt);
    check(dashResult.events.didDash, "dash event emits once");
    check(lengthXY(dash.baseVelocity) < 50.0f,
          "friction runs after dash");
    dashCommand.dashPressed = false;
    dashResult = simulateMovementStepWithSpecials(
        dash, dashCommand, config, collisionFor(false), kDt);
    check(!dashResult.events.didDash && !dash.dash.didDash,
          "dash event clears next tick");

    MovementState down = defaultState(false);
    MovementCommand downCommand = commandFor();
    downCommand.downDashPressed = true;
    MovementStepEvents preEvents;
    applyPreCollisionBasicMovement(down, commandFor(), config, kDt);
    applySpecialMovementPreCollision(down, downCommand, config, kDt, preEvents);
    check(preEvents.didDownDash, "down dash event emits pre collision");
    checkNear(down.baseVelocity.z, -100.0f, kTolerance,
              "down dash reaches collision same tick");

    MovementState freeze = defaultState(false);
    freeze.baseVelocity = glm::vec3(50.0f, 0.0f, 3.0f);
    freeze.externalImpulse = glm::vec3(10.0f, 0.0f, 7.0f);
    MovementCommand freezeCommand = commandFor();
    freezeCommand.freezeHeld = true;
    freezeCommand.freezePressed = true;
    preEvents = {};
    applyPreCollisionBasicMovement(freeze, freezeCommand, config, kDt);
    applySpecialMovementPreCollision(freeze, freezeCommand, config, kDt, preEvents);
    const MovementVelocityView freezeView =
        movementVelocityViewForCollision(freeze, config);
    check(preEvents.freezeStarted, "freeze start emits in pre-collision phase");
    checkNear(freezeView.effectiveBaseVelocity.x, 0.0f, kTolerance,
              "freeze effective base X reaches collision same tick");
    checkNear(freezeView.effectiveExternalImpulse.x, 0.0f, kTolerance,
              "freeze effective impulse X reaches collision same tick");
    checkNear(freezeView.effectiveBaseVelocity.z, 3.0f + config.gravityZ * kDt,
              kTolerance,
              "freeze effective base Z reaches collision unsuppressed");
    checkNear(freezeView.effectiveExternalImpulse.z, 7.0f, kTolerance,
              "freeze effective impulse Z reaches collision unsuppressed");

    MovementState protectedDash = defaultState(false);
    MovementCommand moveDash = commandFor(glm::vec2(0.0f, 1.0f));
    moveDash.dashPressed = true;
    moveDash.movementDirectionFreshPressed = true;
    simulateMovementStepWithSpecials(protectedDash, moveDash, config, collisionFor(false), kDt);
    moveDash.dashPressed = false;
    moveDash.movementDirectionFreshPressed = false;
    simulateMovementStepWithSpecials(protectedDash, moveDash, config, collisionFor(false), kDt);
    check(lengthXY(protectedDash.baseVelocity) > 50.0f,
          "same held movement does not erase dash in post-collision phase");

    MovementCommand changed = commandFor(glm::vec2(1.0f, 0.0f));
    changed.movementDirectionChanged = true;
    simulateMovementStepWithSpecials(protectedDash, changed, config, collisionFor(false), kDt);
    checkNear(protectedDash.baseVelocity.x, 20.0f, 0.001f,
              "changed movement overwrites protected dash in post-collision phase");

    MovementState jumpDown = defaultState(true);
    MovementCommand both = commandFor();
    both.jumpPressed = true;
    both.downDashPressed = true;
    simulateMovementStepWithSpecials(jumpDown, both, config, collisionFor(true), kDt);
    checkNear(jumpDown.baseVelocity.z, config.jumpVerticalSpeed, kTolerance,
              "jump after grounded down dash has deterministic final Z");
}

void fillSpecialPlayer(Player& player)
{
    player.spawnGeneration = 12;
    player.pos = glm::vec3(1.0f, 2.0f, 3.0f);
    player.vel = glm::vec3(4.0f, 5.0f, 6.0f);
    player.externalImpulse = glm::vec3(7.0f, 8.0f, 9.0f);
    player.inputWishMove = glm::vec2(0.0f, 1.0f);
    player.yaw = 45.0f;
    player.sizeScale = 1.25f;
    player.dash.dashAvailable = false;
    player.dash.downDashAvailable = false;
    player.dash.didDownDash = true;
    player.dash.momentumProtectionActive = true;
    player.dash.momentumProtectionUsedCameraForwardFallback = false;
    player.dash.momentumProtectedMoveAxes = glm::vec2(0.0f, 1.0f);
    player.dash.movementInputGeneration = 9;
    player.freeze.freezeActive = true;
    player.freeze.freezeAvailable = false;
    player.freeze.freezeHeldPrev = true;
    player.freeze.freezeTimer = 1.5f;
}

void testConversion()
{
    Player player(false);
    fillSpecialPlayer(player);
    player.currentHp = 31;
    player.dead = true;
    player.equippedSlot = 4;
    player.username = "special-state";

    MovementState state = movementStateFromPlayer(player, MovementLifecycleIdentity{12, 3});
    check(!state.dash.dashAvailable, "conversion maps dash availability");
    check(!state.downDash.available, "conversion maps down dash availability");
    check(state.freeze.active && !state.freeze.available,
          "conversion maps freeze active and availability");
    checkNear(state.freeze.timerSeconds, 1.5f, kTolerance,
              "conversion maps freeze timer");
    check(state.freeze.heldPreviously, "conversion maps freeze prior-held state");
    check(state.dashMomentumProtection.active,
          "conversion maps dash momentum protection");
    checkVec2(state.dashMomentumProtection.protectedMoveAxes,
              glm::vec2(0.0f, 1.0f),
              "conversion maps dash protection axes");
    check(state.dashMomentumProtection.movementInputGeneration == 9,
          "conversion maps dash protection generation");

    state.dash.dashAvailable = true;
    MovementCommand command = commandFor(glm::vec2(1.0f, 0.0f));
    command.dashPressed = true;
    MovementStepEvents events;
    tryActivateDash(state, command, currentConfig(), events);

    Player target(false);
    target.currentHp = 88;
    target.dead = true;
    target.equippedSlot = 5;
    target.username = "unchanged";
    applyMovementStateToPlayer(state, target);

    check(target.dash.dashAvailable == state.dash.dashAvailable,
          "apply preserves dash availability");
    check(target.dash.downDashAvailable == state.downDash.available,
          "apply preserves down dash availability");
    check(target.freeze.freezeActive == state.freeze.active,
          "apply preserves freeze active");
    check(target.freeze.freezeAvailable == state.freeze.available,
          "apply preserves freeze availability");
    checkNear(target.freeze.freezeTimer, state.freeze.timerSeconds, kTolerance,
              "apply preserves freeze timer");
    check(target.freeze.freezeHeldPrev == state.freeze.heldPreviously,
          "apply preserves freeze prior-held state");
    check(target.dash.momentumProtectionActive ==
              state.dashMomentumProtection.active,
          "apply preserves dash momentum protection");
    check(target.currentHp == 88, "apply leaves health untouched");
    check(target.dead, "apply leaves death flag untouched");
    check(target.equippedSlot == 5, "apply leaves equipped slot untouched");
    check(target.username == "unchanged", "apply leaves username untouched");
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
    state.lifecycle = MovementLifecycleIdentity{static_cast<uint32_t>(1000 + index), 7};
    state.baseVelocity = glm::vec3(randomFloat(rng, -100.0f, 100.0f),
                                   randomFloat(rng, -100.0f, 100.0f),
                                   randomFloat(rng, -120.0f, 80.0f));
    state.externalImpulse = glm::vec3(randomFloat(rng, -300.0f, 300.0f),
                                      randomFloat(rng, -300.0f, 300.0f),
                                      randomFloat(rng, -80.0f, 120.0f));
    state.dash.dashAvailable = randomBool(rng);
    state.downDash.available = randomBool(rng);
    state.freeze.active = randomBool(rng);
    state.freeze.available = randomBool(rng);
    state.freeze.timerSeconds = randomFloat(rng, 0.0f, 6.0f);
    state.freeze.heldPreviously = randomBool(rng);
    state.dashMomentumProtection.active = randomBool(rng);
    state.dashMomentumProtection.protectedMoveAxes = movementClampUnitOrZero(
        glm::vec2(randomFloat(rng, -1.0f, 1.0f), randomFloat(rng, -1.0f, 1.0f)));
    state.dashMomentumProtection.usedCameraForwardFallback = randomBool(rng);
    state.sizeScale = randomFloat(rng, 0.2f, 4.0f);
    return state;
}

MovementCommand randomCommand(std::mt19937& rng)
{
    MovementCommand command;
    command.moveAxes = movementClampUnitOrZero(
        glm::vec2(randomFloat(rng, -2.0f, 2.0f), randomFloat(rng, -2.0f, 2.0f)));
    command.horizontalCameraForward = glm::vec3(randomFloat(rng, -1.0f, 1.0f),
                                                randomFloat(rng, -1.0f, 1.0f),
                                                randomFloat(rng, -4.0f, 4.0f));
    command.lookYaw = randomFloat(rng, -360.0f, 360.0f);
    command.dashPressed = randomBool(rng);
    command.downDashPressed = randomBool(rng);
    command.freezeHeld = randomBool(rng);
    command.freezePressed = randomBool(rng);
    if (command.freezePressed)
        command.freezeHeld = true;
    command.movementDirectionPressed = movementHasMoveInput(command.moveAxes);
    command.movementDirectionFreshPressed = randomBool(rng);
    command.movementDirectionReleased = randomBool(rng);
    command.movementDirectionChanged = randomBool(rng);
    return command;
}

void testRandomizedInvariants()
{
    std::mt19937 rng(kRandomSeed);
    const MovementConfig config = currentConfig();
    const int failuresBefore = gFailures;

    for (int i = 0; i < kRandomCases; ++i) {
        const MovementState initial = randomState(rng, i);
        const MovementCommand command = randomCommand(rng);

        {
            MovementState state = initial;
            MovementStepEvents events;
            const glm::vec3 beforeBase = state.baseVelocity;
            const glm::vec3 beforeImpulse = state.externalImpulse;
            const bool activated = tryActivateDash(state, command, config, events);
            const float passThrough = freezeHorizontalPassThrough(initial.freeze, config);
            const bool expected =
                command.dashPressed &&
                initial.dash.dashAvailable &&
                !(initial.freeze.active && passThrough <= config.freezeDashMinimumPassThrough);

            std::ostringstream context;
            context << "random dash case=" << i;
            check(activated == expected, context.str() + " activation eligibility");
            check(events.didDash == activated, context.str() + " success event matches activation");
            if (activated) {
                const glm::vec2 direction = movementDashDirection(command);
                checkNear(state.baseVelocity.x,
                          beforeBase.x + direction.x * config.dashHorizontalImpulse,
                          0.001f,
                          context.str() + " additive X");
                checkNear(state.baseVelocity.y,
                          beforeBase.y + direction.y * config.dashHorizontalImpulse,
                          0.001f,
                          context.str() + " additive Y");
                checkNear(state.baseVelocity.z, beforeBase.z, kTolerance,
                          context.str() + " preserves Z");
                checkVec3(state.externalImpulse, beforeImpulse,
                          context.str() + " preserves external impulse");
            } else {
                checkVec3(state.baseVelocity, beforeBase,
                          context.str() + " rejected dash preserves velocity");
                checkVec3(state.externalImpulse, beforeImpulse,
                          context.str() + " rejected dash preserves impulse");
            }
            check(movementIsFinite(state), context.str() + " state remains finite");
        }

        {
            MovementState state = initial;
            MovementStepEvents events;
            const glm::vec3 beforeBase = state.baseVelocity;
            const glm::vec3 beforeImpulse = state.externalImpulse;
            const bool activated = tryActivateDownDash(state, command, config, events);
            const bool expected = command.downDashPressed && initial.downDash.available;
            std::ostringstream context;
            context << "random down dash case=" << i;
            check(activated == expected, context.str() + " activation eligibility");
            check(events.didDownDash == activated,
                  context.str() + " success event matches activation");
            if (activated) {
                checkNear(state.baseVelocity.x, beforeBase.x, kTolerance,
                          context.str() + " preserves X");
                checkNear(state.baseVelocity.y, beforeBase.y, kTolerance,
                          context.str() + " preserves Y");
                checkNear(state.baseVelocity.z, config.downDashVerticalSpeed, kTolerance,
                          context.str() + " sets Z");
                checkVec3(state.externalImpulse, beforeImpulse,
                          context.str() + " preserves external impulse");
            } else {
                checkVec3(state.baseVelocity, beforeBase,
                          context.str() + " rejected down dash preserves velocity");
                checkVec3(state.externalImpulse, beforeImpulse,
                          context.str() + " rejected down dash preserves impulse");
            }
            check(movementIsFinite(state), context.str() + " state remains finite");
        }

        {
            MovementState state = initial;
            MovementStepEvents events;
            const glm::vec3 beforeImpulse = state.externalImpulse;
            updateFreeze(state, command, config, kDt, events);
            std::ostringstream context;
            context << "random freeze case=" << i;
            if (events.freezeStarted) {
                checkNear(state.baseVelocity.x, 0.0f, kTolerance,
                          context.str() + " start zeros base X");
                checkNear(state.baseVelocity.y, 0.0f, kTolerance,
                          context.str() + " start zeros base Y");
                checkVec3(state.externalImpulse, beforeImpulse,
                          context.str() + " start preserves impulse");
                check(state.freeze.active && !state.freeze.available,
                      context.str() + " start consumes availability");
            }
            if (!initial.freeze.available &&
                (command.freezePressed || (command.freezeHeld && !initial.freeze.heldPreviously)) &&
                !events.freezeStarted) {
                checkVec3(state.externalImpulse, beforeImpulse,
                          context.str() + " unavailable start preserves impulse");
            }
            check(movementIsFinite(state), context.str() + " state remains finite");
        }

        {
            MovementState state = initial;
            MovementCollisionFeedback collision = collisionFor(randomBool(rng));
            MovementStepResult result =
                simulateMovementStepWithSpecials(state, command, config, collision, kDt);
            std::ostringstream context;
            context << "random full step case=" << i;
            const bool freezeEdge =
                command.freezePressed ||
                (command.freezeHeld && !initial.freeze.heldPreviously);
            check(movementIsFinite(result.state), context.str() + " result finite");
            check(movementIsFinite(state), context.str() + " mutated state finite");
            check(!(result.events.didDash && !command.dashPressed),
                  context.str() + " dash event requires edge command");
            check(!(result.events.didDownDash && !command.downDashPressed),
                  context.str() + " down dash event requires edge command");
            check(!(result.events.freezeStarted && !freezeEdge),
                  context.str() + " freeze start requires edge command");
        }
    }

    check(gFailures == failuresBefore,
          "randomized invariants had no mismatches for seed 0x2B2026 across 512 cases");
}

} // namespace

int main()
{
    testDashDirection();
    testDashForce();
    testDashEdgeAvailability();
    testDashMomentumProtection();
    testDownDash();
    testFreezeActivationCurveAndVertical();
    testFreezeStorageAndRecharge();
    testFreezeAndDashInteraction();
    testEventsAndPhaseOrder();
    testConversion();
    testRandomizedInvariants();

    if (gFailures != 0) {
        std::cerr << "[movement-special-kernel-test] FAIL failures=" << gFailures
                  << " seed=" << kRandomSeed
                  << " cases=" << kRandomCases
                  << " maxDifference=" << gMaxDifference << "\n";
        return 1;
    }

    std::cout << "[movement-special-kernel-test] PASS seed=" << kRandomSeed
              << " randomizedCases=" << kRandomCases
              << " maxDifference<=" << gMaxDifference << "\n";
    return 0;
}
