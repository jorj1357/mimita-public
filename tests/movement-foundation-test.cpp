// 07 21 2026, 15 45
/* purpose
* Verifies Stage 1 shared movement data construction, helpers, and conversion boundaries.
* Guards Player and server mapping so unsupported parity is explicit instead of invented.
* Tests deterministic freeze curve, lifecycle matching, movement direction semantics, and config values.
* Does NOT run live physics, networking transport, rendering, audio, or weapon simulation.
* Does NOT launch mimita.exe or require a graphics window.
* Does NOT validate future client prediction history or packet protocol changes.
*/

#include "physics/movement/movement-types.h"

#include <cmath>
#include <iostream>
#include <string>

#include "combat/weapon-runtime.h"
#include "config/player-settings.h"
#include "entities/player.h"
#include "network/server.h"
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

int gFailures = 0;

void check(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "[FAIL] " << message << "\n";
        ++gFailures;
    }
}

void checkNear(float actual, float expected, float tolerance, const char* message)
{
    if (std::fabs(actual - expected) > tolerance) {
        std::cerr << "[FAIL] " << message << " expected=" << expected
                  << " actual=" << actual << "\n";
        ++gFailures;
    }
}

void checkVec2(glm::vec2 actual, glm::vec2 expected, const char* message)
{
    checkNear(actual.x, expected.x, 0.0001f, message);
    checkNear(actual.y, expected.y, 0.0001f, message);
}

void checkVec3(glm::vec3 actual, glm::vec3 expected, const char* message)
{
    checkNear(actual.x, expected.x, 0.0001f, message);
    checkNear(actual.y, expected.y, 0.0001f, message);
    checkNear(actual.z, expected.z, 0.0001f, message);
}

void testDefaults()
{
    MovementCommand command;
    MovementState state;
    MovementContact contact;
    MovementExternalEvent event;
    MovementStepResult result;

    check(command.sequence == 0, "command sequence defaults to zero");
    checkVec2(command.moveAxes, glm::vec2(0.0f), "command move axes default to zero");
    check(command.jumpHeld == false, "command jump held defaults false");
    check(state.movementEnabled == true, "movement state defaults enabled");
    checkVec3(state.ground.groundNormal, glm::vec3(0.0f, 0.0f, 1.0f),
              "ground normal defaults up");
    check(state.dash.dashAvailable == true, "dash availability default matches runtime new-life state");
    check(state.freeze.available == true, "freeze availability default matches runtime new-life state");
    check(contact.kind == MovementContactKind::Unknown, "contact kind defaults unknown");
    checkVec3(contact.normal, glm::vec3(0.0f, 0.0f, 1.0f), "contact normal defaults up");
    check(event.type == MovementExternalEventType::AddImpulse,
          "external event defaults to add impulse");
    check(result.events.contacts.empty(), "step result contacts default empty");
}

void testCommandConversion()
{
    InputFrame frame;
    frame.moveX = -1.0f;
    frame.moveY = 0.0f;
    frame.jump = true;
    frame.jumpPressed = true;
    frame.dashPressed = true;
    frame.movementPressed = true;
    frame.movementJustPressed = true;
    frame.groundReturnPressed = true;
    frame.downDashPressed = true;
    frame.freezeHeld = true;
    frame.lookYaw = 90.0f;
    frame.lookPitch = -12.0f;

    InputState input;
    input.wishMoveXY = glm::vec2(0.6f, 0.8f);
    input.camForward = glm::vec3(0.0f, 4.0f, 2.0f);
    input.jumpHeld = true;
    input.jumpPressed = true;
    input.dashPressed = true;
    input.movementPressed = true;
    input.movementJustPressed = true;
    input.groundReturnPressed = true;
    input.downDashPressed = true;
    input.freezeHeld = true;
    input.movementHeldDuration = 0.25f;

    MovementCommand command = movementCommandFromInput(
        frame, input, 42, 900, MovementLifecycleIdentity{7, 3});

    check(command.sequence == 42, "command preserves sequence");
    check(command.clientSimulationTick == 900, "command preserves simulation tick");
    check(sameMovementLifecycle(command.lifecycle, MovementLifecycleIdentity{7, 3}),
          "command preserves lifecycle");
    checkVec2(command.moveAxes, glm::vec2(0.6f, 0.8f), "command uses canonical input axes");
    checkVec3(command.horizontalCameraForward, glm::vec3(0.0f, 1.0f, 0.0f),
              "command flattens and normalizes camera forward");
    checkNear(command.lookYaw, 90.0f, 0.0001f, "command preserves yaw");
    checkNear(command.lookPitch, -12.0f, 0.0001f, "command preserves pitch");
    check(command.jumpHeld, "command maps jump held");
    check(command.jumpPressed, "command maps jump pressed");
    check(command.dashPressed, "command maps dash press");
    check(command.downDashPressed, "command maps down dash press");
    check(command.groundReturnPressed, "command maps ground return press");
    check(command.freezeHeld, "command maps freeze held");
    check(command.movementDirectionPressed, "command maps movement pressed");
    check(command.movementDirectionFreshPressed, "command maps movement fresh press");
    check(command.movementDirectionChanged, "command maps current direction-change edge");
    checkNear(command.movementHeldDurationSeconds, 0.25f, 0.0001f,
              "command preserves movement held duration");
    check(movementIsFinite(command), "command validates finite values");
}

void fillPlayerMovementFields(Player& player)
{
    player.spawnGeneration = 11;
    player.pos = glm::vec3(1.0f, 2.0f, 3.0f);
    player.vel = glm::vec3(4.0f, 5.0f, 6.0f);
    player.externalImpulse = glm::vec3(7.0f, 8.0f, 9.0f);
    player.inputWishMove = glm::vec2(0.25f, -0.5f);
    player.yaw = 33.0f;
    player.sizeScale = 1.75f;

    player.ground.onGround = true;
    player.ground.stableOnGround = true;
    player.ground.wasOnGround = false;
    player.ground.hasWorldContact = true;
    player.ground.realWorldContactThisFrame = true;
    player.ground.groundLostTimer = 0.1f;
    player.ground.airborneTimer = 0.2f;
    player.ground.landingCooldown = 0.3f;
    player.ground.worldContactLostTimer = 0.4f;

    player.jump.airJumpsLeft = 1;
    player.jump.jumpHeldPrev = true;
    player.jump.airJumpLocked = true;
    player.jump.airJumpArmed = true;
    player.jump.jumpIntentTimer = 0.5f;
    player.jump.coyoteTimer = 0.6f;
    player.jump.didGroundJump = true;
    player.jump.didAirJump = true;

    player.dash.dashAvailable = false;
    player.dash.downDashAvailable = false;
    player.dash.dashHeldPrev = true;
    player.dash.moveHeldPrev = true;
    player.dash.dashMovementTicks = 3;
    player.dash.lastDashQuality = 2;
    player.dash.didDash = true;
    player.dash.didDownDash = true;
    player.dash.frictionOverride = 0.5f;
    player.dash.tickPerfectDash = true;

    player.freeze.freezeAvailable = false;
    player.freeze.freezeHeldPrev = true;
    player.freeze.freezeActive = true;
    player.freeze.freezeTimer = 1.25f;
    player.freeze.didFreeze = true;

    player.groundReturn.available = false;
    player.groundReturn.charges = 2;
    player.groundReturn.rechargeTimer = 0.75f;
}

void testPlayerMappingAndApply()
{
    Player player(false);
    fillPlayerMovementFields(player);
    player.currentHp = 37;
    player.dead = true;
    player.equippedSlot = 4;
    player.username = "movement-test";

    MovementState state = movementStateFromPlayer(player, MovementLifecycleIdentity{11, 22});
    check(!state.movementEnabled, "player mapping carries movement enabled from death state");
    check(sameMovementLifecycle(state.lifecycle, MovementLifecycleIdentity{11, 22}),
          "player mapping preserves supplied lifecycle");
    checkVec3(state.position, glm::vec3(1.0f, 2.0f, 3.0f), "player maps position");
    checkVec3(state.baseVelocity, glm::vec3(4.0f, 5.0f, 6.0f), "player maps velocity");
    checkVec3(state.externalImpulse, glm::vec3(7.0f, 8.0f, 9.0f),
              "player maps external impulse");
    checkVec2(state.lastInputMoveAxes, glm::vec2(0.25f, -0.5f),
              "player maps last input axes");
    check(state.ground.realWorldContactThisFrame, "player maps real world contact");
    check(state.jump.airJumpLocked, "player maps air jump lock");
    check(state.dash.tickPerfectDash, "player maps tick-perfect dash");
    check(!state.downDash.available, "player maps down dash availability");
    check(state.freeze.active, "player maps freeze active");
    check(state.groundReturn.charges == 2, "player maps ground return charges");
    check(!state.dashMomentumProtection.active,
          "player mapping does not invent dash momentum protection state");

    Player target(false);
    target.currentHp = 88;
    target.dead = true;
    target.equippedSlot = 5;
    target.username = "unchanged";
    applyMovementStateToPlayer(state, target);

    check(target.spawnGeneration == 11, "apply updates player spawn generation");
    checkVec3(target.pos, state.position, "apply writes player position");
    checkVec3(target.vel, state.baseVelocity, "apply writes player velocity");
    checkVec3(target.externalImpulse, state.externalImpulse, "apply writes external impulse");
    check(target.ground.onGround == state.ground.onGround, "apply writes ground state");
    check(target.jump.airJumpsLeft == state.jump.airJumpsLeft, "apply writes jump state");
    check(target.dash.downDashAvailable == state.downDash.available,
          "apply writes down dash state");
    check(target.freeze.freezeTimer == state.freeze.timerSeconds,
          "apply writes freeze timer");
    check(target.groundReturn.rechargeTimer == state.groundReturn.rechargeTimerSeconds,
          "apply writes ground return timer");
    check(target.currentHp == 88, "apply leaves health untouched");
    check(target.dead == true, "apply leaves death/lifecycle gameplay untouched");
    check(target.equippedSlot == 5, "apply leaves weapon slot untouched");
    check(target.username == "unchanged", "apply leaves identity string untouched");

    MovementState roundTrip = movementStateFromPlayer(target, state.lifecycle);
    checkVec3(roundTrip.position, state.position, "round trip preserves position");
    checkVec3(roundTrip.baseVelocity, state.baseVelocity, "round trip preserves velocity");
    checkVec3(roundTrip.externalImpulse, state.externalImpulse,
              "round trip preserves external impulse");
    check(roundTrip.freeze.active == state.freeze.active, "round trip preserves freeze active");
    check(movementIsFinite(roundTrip), "round trip state validates finite values");
}

void testServerMapping()
{
    MimitaNet::ServerPlayer serverPlayer;
    serverPlayer.spawnGeneration = 44;
    serverPlayer.transformEpoch = 9;
    serverPlayer.spawnState = MimitaNet::ServerPlayer::Active;
    serverPlayer.pos = glm::vec3(10.0f, 11.0f, 12.0f);
    serverPlayer.vel = glm::vec3(13.0f, 14.0f, 15.0f);
    serverPlayer.yaw = 270.0f;
    serverPlayer.sizeScale = 0.8f;
    serverPlayer.onGround = true;
    serverPlayer.dashAvailable = false;
    serverPlayer.input.wish = glm::vec2(0.0f, 2.0f);
    serverPlayer.input.camForward = glm::vec3(-2.0f, 0.0f, 5.0f);
    serverPlayer.input.yaw = 180.0f;
    serverPlayer.input.jumpHeld = true;
    serverPlayer.input.dashPressed = true;
    serverPlayer.input.freezeHeld = true;
    serverPlayer.input.tick = 1234;
    serverPlayer.health = 23;
    serverPlayer.dead = false;

    MovementState state = movementStateFromServerPlayer(serverPlayer);
    check(sameMovementLifecycle(state.lifecycle, MovementLifecycleIdentity{44, 9}),
          "server mapping preserves lifecycle fields");
    checkVec3(state.position, glm::vec3(10.0f, 11.0f, 12.0f),
              "server maps position");
    checkVec3(state.baseVelocity, glm::vec3(13.0f, 14.0f, 15.0f),
              "server maps velocity");
    checkVec3(state.externalImpulse, glm::vec3(0.0f),
              "server mapping leaves unsupported external impulse zero");
    check(state.ground.onGround, "server maps ground bool");
    check(!state.dash.dashAvailable, "server maps dash availability");
    check(state.jump.airJumpsLeft == 0, "server mapping does not invent air jump state");
    check(!state.downDash.didDownDash, "server mapping does not invent down dash event");
    check(state.freeze.heldPreviously, "server maps available freeze input hint");

    MovementCommand command = movementCommandFromServerInput(
        serverPlayer.input, 77, state.lifecycle);
    check(command.sequence == 77, "server input command preserves sequence");
    check(command.clientSimulationTick == 1234, "server input command maps tick");
    checkVec2(command.moveAxes, glm::vec2(0.0f, 1.0f),
              "server input command clamps wish axes");
    checkVec3(command.horizontalCameraForward, glm::vec3(-1.0f, 0.0f, 0.0f),
              "server input command flattens camera forward");
    check(command.jumpHeld, "server input command maps jump held");
    check(command.dashPressed, "server input command maps dash pressed");
    check(command.freezeHeld, "server input command maps freeze held");

    MovementState nextState = state;
    nextState.lifecycle = MovementLifecycleIdentity{45, 10};
    nextState.position = glm::vec3(-1.0f, -2.0f, -3.0f);
    nextState.baseVelocity = glm::vec3(-4.0f, -5.0f, -6.0f);
    nextState.lastInputMoveAxes = glm::vec2(0.25f, 0.25f);
    nextState.yaw = 91.0f;
    nextState.sizeScale = 1.4f;
    nextState.ground.onGround = false;
    nextState.dash.dashAvailable = true;
    applyMovementStateToServerPlayer(nextState, serverPlayer);

    check(serverPlayer.spawnGeneration == 45, "server apply writes spawn generation");
    check(serverPlayer.transformEpoch == 10, "server apply writes transform epoch");
    checkVec3(serverPlayer.pos, nextState.position, "server apply writes position");
    checkVec3(serverPlayer.vel, nextState.baseVelocity, "server apply writes velocity");
    check(serverPlayer.onGround == false, "server apply writes ground bool");
    check(serverPlayer.dashAvailable, "server apply writes dash availability");
    check(serverPlayer.health == 23, "server apply leaves health untouched");
    check(serverPlayer.dead == false, "server apply leaves death flag untouched");

    MovementServerConversionSupport support = currentServerMovementConversionSupport();
    check(support.position && support.baseVelocity && support.lifecycleTransformEpoch,
          "server conversion support reports mapped fields");
    check(!support.externalImpulse && !support.jumpTimers && !support.downDashState &&
              !support.freezeTimerState,
          "server conversion support reports parity gaps");
}

void testConfigAndFreezeCurve()
{
    MovementConfig config = makeCurrentRuntimeMovementConfig();
    check(config.simulationHz == 60, "config maps simulation hz");
    checkNear(config.fixedDeltaSeconds, 1.0f / 60.0f, 0.000001f,
              "config maps fixed delta");
    checkNear(config.groundSpeed, 20.0f, 0.0001f, "config maps ground speed");
    checkNear(config.airSpeed, 20.0f, 0.0001f, "config maps air speed");
    checkNear(config.gravityZ, -58.0f, 0.0001f, "config maps gravity");
    checkNear(config.jumpVerticalSpeed, 19.0f, 0.0001f, "config maps jump speed");
    checkNear(config.groundDashImpulse, 100.0f, 0.0001f,
              "config maps current ground/server dash impulse");
    checkNear(config.airDashImpulse, 50.0f, 0.0001f,
              "config maps current local air dash impulse");
    checkNear(config.downDashVerticalSpeed, -100.0f, 0.0001f,
              "config maps down dash speed");
    checkNear(config.freezeDurationSeconds, 5.0f, 0.0001f,
              "config maps freeze duration");
    checkNear(config.freezeCurveExponent, 4.0f, 0.0001f,
              "config uses target quartic freeze curve exponent");

    checkNear(movementFreezeHorizontalPassThrough(0.0f), 0.0f, 0.000001f,
              "freeze curve at 0 seconds");
    checkNear(movementFreezeHorizontalPassThrough(1.0f), 0.0016f, 0.000001f,
              "freeze curve at 1 second");
    checkNear(movementFreezeHorizontalPassThrough(2.0f), 0.0256f, 0.000001f,
              "freeze curve at 2 seconds");
    checkNear(movementFreezeHorizontalPassThrough(3.0f), 0.1296f, 0.000001f,
              "freeze curve at 3 seconds");
    checkNear(movementFreezeHorizontalPassThrough(4.0f), 0.4096f, 0.000001f,
              "freeze curve at 4 seconds");
    checkNear(movementFreezeHorizontalPassThrough(5.0f), 1.0f, 0.000001f,
              "freeze curve at 5 seconds");
    checkNear(movementFreezeHorizontalPassThrough(6.0f), 1.0f, 0.000001f,
              "freeze curve clamps over duration");
    checkNear(movementFreezeHorizontalPassThrough(-1.0f), 0.0f, 0.000001f,
              "freeze curve clamps negative time");
}

void testLifecycleDirectionAndExternalEvents()
{
    MovementLifecycleIdentity life{3, 8};
    MovementLifecycleIdentity otherLife{4, 8};
    MovementExternalEvent event;
    event.targetLifecycle = life;
    event.eventId = 99;
    event.sourceEntityId = 1001;
    event.type = MovementExternalEventType::AddImpulse;
    event.vector = glm::vec3(9.0f, 0.0f, 2.0f);
    event.authoritative = true;

    check(movementEventMatchesLifecycle(event, life), "external event matches target lifecycle");
    check(!movementEventMatchesLifecycle(event, otherLife),
          "external event rejects stale lifecycle");
    check(event.type == MovementExternalEventType::AddImpulse,
          "external event preserves type");
    checkVec3(event.vector, glm::vec3(9.0f, 0.0f, 2.0f),
              "external event preserves vector");

    MovementDirectionTransition first =
        movementDirectionTransition(glm::vec2(0.0f), glm::vec2(0.0f, 1.0f));
    check(first.freshPress, "direction no-input to W is fresh press");
    check(first.meaningfulChange, "direction no-input to W is meaningful");

    MovementDirectionTransition held =
        movementDirectionTransition(glm::vec2(0.0f, 1.0f), glm::vec2(0.0f, 1.0f));
    check(!held.freshPress && !held.directionChanged && !held.released,
          "direction W held stays unchanged");

    MovementDirectionTransition diagonal =
        movementDirectionTransition(glm::vec2(0.0f, 1.0f), glm::vec2(1.0f, 1.0f));
    check(diagonal.directionChanged, "direction W to W+A is a direction change");
    check(diagonal.meaningfulChange, "direction W to W+A is meaningful");

    MovementDirectionTransition release =
        movementDirectionTransition(glm::vec2(1.0f, 1.0f), glm::vec2(0.0f));
    check(release.released, "direction W+A to no input is release");
    check(!release.freshPress, "direction release is not fresh press");

    MovementDirectionTransition second =
        movementDirectionTransition(glm::vec2(0.0f), glm::vec2(1.0f, 0.0f));
    check(second.freshPress, "direction no-input to D is fresh press");
}

} // namespace

int main()
{
    testDefaults();
    testCommandConversion();
    testPlayerMappingAndApply();
    testServerMapping();
    testConfigAndFreezeCurve();
    testLifecycleDirectionAndExternalEvents();

    if (gFailures != 0) {
        std::cerr << "[movement-foundation-test] FAIL failures=" << gFailures << "\n";
        return 1;
    }

    std::cout << "[movement-foundation-test] PASS\n";
    return 0;
}
