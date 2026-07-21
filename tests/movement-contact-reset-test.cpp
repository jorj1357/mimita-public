// 07 21 2026, 17 25
/* purpose
* Verifies Stage 2C shared movement contact reset semantics.
* Exercises bounded contact sets, lifecycle filtering, stable-event dedupe, and phase order.
* Proves contact reset restores abilities without mutating momentum or presentation systems.
* Does NOT launch mimita.exe, render, play audio, send packets, or open sockets.
* Does NOT test projectile damage, weapon firing, ICE, interpolation, or reconciliation.
* Does NOT fake runtime dynamic contact wiring beyond pure neutral contact facts.
*/

#include "physics/movement/movement-step.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <string>

namespace {

constexpr uint32_t kRandomSeed = 0x2C2026u;
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
        out << message << " expected=" << expected
            << " actual=" << actual
            << " diff=" << (actual - expected);
        fail(out.str());
    }
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

MovementLifecycleIdentity life(uint32_t spawn = 7, uint32_t epoch = 3)
{
    return MovementLifecycleIdentity{spawn, epoch};
}

MovementConfig testConfig()
{
    MovementConfig config;
    config.fixedDeltaSeconds = kDt;
    config.maximumDeltaSeconds = 0.033f;
    config.groundSpeed = 20.0f;
    config.airSpeed = 20.0f;
    config.gravityZ = -58.0f;
    config.maximumFallSpeed = 120.0f;
    config.jumpVerticalSpeed = 19.0f;
    config.jumpBufferSeconds = 0.2f;
    config.coyoteSeconds = 0.1f;
    config.maximumAirJumps = 1;
    config.dashHorizontalImpulse = 50.0f;
    config.downDashVerticalSpeed = -100.0f;
    config.freezeDurationSeconds = 5.0f;
    config.freezeCurveExponent = 4.0f;
    config.freezeDashMinimumPassThrough = 0.001f;
    config.maximumExternalImpulseSpeed = 1000.0f;
    config.externalImpulseDecay = 0.0f;
    config.groundFrictionAmount = 0.0f;
    config.airFrictionAmount = 0.0f;
    config.almostZeroSpeed = 0.00001f;
    config.walkableSlopeDot = 0.7f;
    config.stableGroundGraceSeconds = 0.08f;
    config.landingMinimumAirborneSeconds = 0.08f;
    config.landingCooldownResetSeconds = 0.3f;
    return config;
}

MovementState defaultState(MovementLifecycleIdentity lifecycle = life())
{
    MovementState state;
    state.lifecycle = lifecycle;
    state.sizeScale = 1.0f;
    state.jump.airJumpsLeft = 1;
    state.jump.airJumpArmed = true;
    state.dash.dashAvailable = true;
    state.dash.frictionOverride = 1.0f;
    state.downDash.available = true;
    state.freeze.available = true;
    state.groundReturn.available = true;
    return state;
}

MovementState unavailableState()
{
    MovementState state = defaultState();
    state.jump.airJumpsLeft = 0;
    state.jump.airJumpArmed = false;
    state.jump.airJumpLocked = true;
    state.dash.dashAvailable = false;
    state.downDash.available = false;
    state.freeze.available = false;
    state.groundReturn.available = false;
    return state;
}

MovementCommand commandFor(MovementLifecycleIdentity lifecycle = life())
{
    MovementCommand command;
    command.lifecycle = lifecycle;
    command.horizontalCameraForward = glm::vec3(1.0f, 0.0f, 0.0f);
    command.lookYaw = 0.0f;
    return command;
}

MovementContact worldContact(MovementContactKind kind,
                             uint64_t tick,
                             MovementLifecycleIdentity lifecycle = life(),
                             glm::vec3 normal = glm::vec3(1.0f, 0.0f, 0.0f),
                             uint32_t surfaceId = 1,
                             bool resets = true)
{
    return makeStaticWorldMovementContact(kind,
                                          tick,
                                          lifecycle,
                                          glm::vec3(2.0f, 3.0f, 4.0f),
                                          normal,
                                          surfaceId,
                                          0.05f,
                                          resets);
}

MovementContactSet oneContact(const MovementContact& contact)
{
    MovementContactSet contacts;
    contacts.addDeduplicated(contact);
    return contacts;
}

MovementCollisionFeedback collisionWith(const MovementContact& contact,
                                        bool onGround = false)
{
    MovementCollisionFeedback collision;
    collision.onGround = onGround;
    collision.hasWorldContact = true;
    collision.realWorldContactThisFrame = true;
    collision.simulationTick = contact.simulationTick;
    collision.groundNormal = contact.normal;
    collision.contacts.addDeduplicated(contact);
    return collision;
}

void testCentralReset()
{
    const MovementConfig config = testConfig();
    MovementState state = unavailableState();
    state.baseVelocity = glm::vec3(9.0f, -8.0f, 7.0f);
    state.externalImpulse = glm::vec3(-6.0f, 5.0f, 4.0f);
    state.dashMomentumProtection.active = true;
    state.dashMomentumProtection.protectedMoveAxes = glm::vec2(0.0f, 1.0f);
    state.freeze.active = true;
    state.freeze.timerSeconds = 4.25f;

    const MovementAbilityResetResult reset = resetTouchAbilities(state, config);
    check(reset.anyRestored, "central reset reports meaningful restoration");
    check(reset.airJumpRestored && reset.dashRestored &&
              reset.downDashRestored && reset.freezeRestored &&
              reset.groundReturnRestored,
          "central reset reports every unavailable ability");
    check(state.jump.airJumpsLeft == config.maximumAirJumps,
          "central reset restores air jump count");
    check(state.jump.airJumpArmed && !state.jump.airJumpLocked,
          "central reset arms and unlocks air jump");
    check(state.dash.dashAvailable && state.downDash.available &&
              state.freeze.available && state.groundReturn.available,
          "central reset restores special availability");
    checkVec3(state.baseVelocity, glm::vec3(9.0f, -8.0f, 7.0f),
              "central reset preserves base velocity");
    checkVec3(state.externalImpulse, glm::vec3(-6.0f, 5.0f, 4.0f),
              "central reset preserves external impulse");
    check(state.dashMomentumProtection.active,
          "central reset preserves dash momentum protection");
    check(state.freeze.active, "central reset does not reactivate or end freeze");
    checkNear(state.freeze.timerSeconds, 4.25f, kTolerance,
              "central reset preserves freeze timer");
}

void testMeaningfulEvents()
{
    const MovementConfig config = testConfig();
    MovementState state = defaultState();
    MovementStepEvents events;
    MovementContactHistory history;
    MovementContactSet contacts = oneContact(
        worldContact(MovementContactKind::Ground, 10, state.lifecycle, glm::vec3(0, 0, 1)));

    MovementContactConsumeResult result =
        consumeMovementContacts(state, config, contacts, history, events);
    check(result.appliedReset, "available contact still applies idempotent reset");
    check(!events.abilitiesReset, "available abilities do not emit false reset event");

    state.dash.dashAvailable = false;
    events = {};
    contacts = oneContact(
        worldContact(MovementContactKind::Ground, 11, state.lifecycle, glm::vec3(0, 0, 1)));
    result = consumeMovementContacts(state, config, contacts, history, events);
    check(events.abilitiesReset, "one unavailable ability emits reset event");
    check(events.dashRestored && !events.downDashRestored &&
              !events.freezeRestored && !events.groundReturnRestored,
          "event flags only restored abilities");
}

void testContactClassification()
{
    const MovementConfig config = testConfig();
    check(classifyMovementContactKindFromNormal(glm::vec3(0, 0, 1), config, true) ==
              MovementContactKind::Ground,
          "ground normal classifies as ground");
    check(classifyMovementContactKindFromNormal(glm::vec3(1, 0, 0), config, false) ==
              MovementContactKind::Wall,
          "horizontal normal classifies as wall");
    check(classifyMovementContactKindFromNormal(glm::vec3(0, 0, -1), config, false) ==
              MovementContactKind::Ceiling,
          "down normal classifies as ceiling");
    check(classifyMovementContactKindFromNormal(glm::vec3(0.8f, 0, 0.3f), config, false) ==
              MovementContactKind::Slope,
          "positive non-walkable normal classifies as slope");
    check(classifyMovementContactKindFromNormal(glm::vec3(1, 0, 0), config, false, true) ==
              MovementContactKind::Step,
          "explicit step wins over normal");
}

void testWorldContactResets()
{
    const MovementConfig config = testConfig();
    for (MovementContactKind kind : {MovementContactKind::Ground,
                                     MovementContactKind::Wall,
                                     MovementContactKind::Ceiling,
                                     MovementContactKind::Slope,
                                     MovementContactKind::Step,
                                     MovementContactKind::StaticWorld}) {
        MovementState state = unavailableState();
        MovementStepEvents events;
        MovementContactHistory history;
        MovementContact contact = worldContact(kind, 20 + static_cast<int>(kind), state.lifecycle);
        MovementContactSet contacts = oneContact(contact);
        consumeMovementContacts(state, config, contacts, history, events);
        check(state.dash.dashAvailable && state.downDash.available &&
                  state.freeze.available && state.jump.airJumpsLeft == 1,
              "world contact restores abilities");
        check(events.contacts.size() == 1, "world contact preserved in events");
    }

    MovementState fallback = unavailableState();
    MovementCommand command = commandFor(fallback.lifecycle);
    MovementCollisionFeedback collision;
    collision.onGround = true;
    collision.hasWorldContact = true;
    collision.realWorldContactThisFrame = true;
    collision.simulationTick = 44;
    collision.groundNormal = glm::vec3(0, 0, 1);
    MovementStepResult result =
        applyPostCollisionMovementWithSpecials(fallback, command, config, collision, kDt);
    check(result.state.dash.dashAvailable &&
              result.state.jump.airJumpsLeft == config.maximumAirJumps,
          "grounded fallback creates shared reset contact");
    check(result.events.contacts.size() == 1 &&
              result.events.contacts[0].kind == MovementContactKind::Ground,
          "grounded fallback reports ground contact");
}

void testMultipleAndStaticDedupe()
{
    const MovementConfig config = testConfig();
    MovementState state = unavailableState();
    MovementStepEvents events;
    MovementContactHistory history;
    MovementContactSet contacts;
    contacts.addDeduplicated(worldContact(MovementContactKind::Ground, 30, state.lifecycle,
                                          glm::vec3(0, 0, 1), 1));
    contacts.addDeduplicated(worldContact(MovementContactKind::Wall, 30, state.lifecycle,
                                          glm::vec3(1, 0, 0), 2));
    contacts.addDeduplicated(worldContact(MovementContactKind::Ceiling, 30, state.lifecycle,
                                          glm::vec3(0, 0, -1), 3));
    contacts.addDeduplicated(worldContact(MovementContactKind::Wall, 30, state.lifecycle,
                                          glm::vec3(1.0f, 0.0f, 0.0f), 2));
    consumeMovementContacts(state, config, contacts, history, events);
    check(contacts.size() == 3 && contacts.duplicateCount == 1,
          "same-tick static duplicates are removed");
    check(events.abilitiesReset && events.qualifyingContactCount == 3,
          "multiple contacts produce one reset event with all facts counted");

    state.dash.dashAvailable = false;
    events = {};
    contacts.clear();
    contacts.addDeduplicated(worldContact(MovementContactKind::Wall, 31, state.lifecycle,
                                          glm::vec3(1, 0, 0), 2));
    consumeMovementContacts(state, config, contacts, history, events);
    check(state.dash.dashAvailable && events.dashRestored,
          "next-tick static contact may reset again");
}

void testDynamicStableDedupe()
{
    const MovementConfig config = testConfig();
    MovementState state = unavailableState();
    MovementStepEvents events;
    MovementContactHistory history;

    MovementContact projectile = makeProjectileMovementContact(
        5001, 99, 40, state.lifecycle, glm::vec3(0), glm::vec3(0, 0, 1));
    consumeMovementContacts(state, config, oneContact(projectile), history, events);
    check(state.dash.dashAvailable && events.abilitiesReset,
          "first stable projectile event restores abilities");

    state.dash.dashAvailable = false;
    events = {};
    projectile.simulationTick = 41;
    consumeMovementContacts(state, config, oneContact(projectile), history, events);
    check(!state.dash.dashAvailable && !events.abilitiesReset,
          "duplicate stable projectile event does not reset twice");

    projectile.sourceEventId = 100;
    projectile.simulationTick = 42;
    events = {};
    consumeMovementContacts(state, config, oneContact(projectile), history, events);
    check(state.dash.dashAvailable && events.dashRestored,
          "different stable event can reset later");
}

void testExplosionAndMomentumSeparation()
{
    const MovementConfig config = testConfig();
    MovementState state = unavailableState();
    MovementStepEvents events;
    MovementContactHistory history;

    MovementExternalEvent impulse;
    impulse.type = MovementExternalEventType::AddImpulse;
    impulse.targetLifecycle = state.lifecycle;
    impulse.eventId = 77;
    impulse.vector = glm::vec3(30.0f, 0.0f, 12.0f);
    state.externalImpulse += impulse.vector;
    events.consumedExternalEvents.push_back(impulse);

    MovementContact explosion = makeExplosionMovementContact(
        77, 9001, 50, state.lifecycle, glm::vec3(5.0f), 2.5f);
    consumeMovementContacts(state, config, oneContact(explosion), history, events);
    check(state.dash.dashAvailable && state.freeze.available,
          "explosion contact restores abilities");
    checkVec3(state.externalImpulse, impulse.vector,
              "explosion reset preserves external impulse");

    state.dash.dashAvailable = false;
    events = {};
    explosion.simulationTick = 51;
    consumeMovementContacts(state, config, oneContact(explosion), history, events);
    check(!state.dash.dashAvailable,
          "duplicate explosion stable event cannot reset again");
}

void testFreezeResetSemantics()
{
    const MovementConfig config = testConfig();
    MovementState state = defaultState();
    state.freeze.available = false;
    state.freeze.active = false;
    state.freeze.heldPreviously = true;
    state.freeze.timerSeconds = config.freezeDurationSeconds;
    MovementStepEvents events;
    MovementContactHistory history;

    consumeMovementContacts(state,
                            config,
                            oneContact(worldContact(MovementContactKind::Wall, 60, state.lifecycle)),
                            history,
                            events);
    check(state.freeze.available, "contact restores freeze availability");

    MovementCommand held = commandFor(state.lifecycle);
    held.freezeHeld = true;
    held.freezePressed = false;
    events = {};
    updateFreeze(state, held, config, kDt, events);
    check(!events.freezeStarted && !state.freeze.active,
          "held E after contact does not auto-restart freeze");
    checkNear(state.freeze.timerSeconds, config.freezeDurationSeconds, kTolerance,
              "held E after contact does not reset timer");

    MovementCommand release = commandFor(state.lifecycle);
    release.freezeHeld = false;
    updateFreeze(state, release, config, kDt, events);

    MovementCommand press = commandFor(state.lifecycle);
    press.freezeHeld = true;
    press.freezePressed = true;
    events = {};
    updateFreeze(state, press, config, kDt, events);
    check(events.freezeStarted && state.freeze.active && !state.freeze.available,
          "fresh E after release starts restored freeze");
    checkNear(state.freeze.timerSeconds, 0.0f, kTolerance,
              "fresh freeze starts at timer zero");
}

void testDashDownDashAndAirJumpPhase()
{
    const MovementConfig config = testConfig();

    {
        MovementState state = defaultState();
        state.dash.dashAvailable = false;
        state.dashMomentumProtection.active = true;
        state.baseVelocity = glm::vec3(5.0f, 6.0f, 7.0f);
        MovementStepEvents events;
        MovementContactHistory history;
        consumeMovementContacts(state,
                                config,
                                oneContact(worldContact(MovementContactKind::Wall, 70, state.lifecycle)),
                                history,
                                events);
        check(state.dash.dashAvailable && state.dashMomentumProtection.active,
              "wall contact restores dash without clearing protection");
        checkVec3(state.baseVelocity, glm::vec3(5.0f, 6.0f, 7.0f),
                  "dash reset does not erase dash velocity");
        MovementCommand dash = commandFor(state.lifecycle);
        dash.moveAxes = glm::vec2(1.0f, 0.0f);
        dash.movementDirectionPressed = true;
        dash.dashPressed = true;
        check(tryActivateDash(state, dash, config, events),
              "fresh Shift can dash after contact reset");
    }

    {
        MovementState state = defaultState();
        state.downDash.available = false;
        state.baseVelocity.z = -30.0f;
        MovementStepEvents events;
        MovementContactHistory history;
        consumeMovementContacts(state,
                                config,
                                oneContact(worldContact(MovementContactKind::Ground, 80, state.lifecycle,
                                                        glm::vec3(0, 0, 1))),
                                history,
                                events);
        check(state.downDash.available, "ground contact restores down dash");
        checkNear(state.baseVelocity.z, -30.0f, kTolerance,
                  "down dash reset does not alter Z");
        MovementCommand down = commandFor(state.lifecycle);
        down.downDashPressed = true;
        check(tryActivateDownDash(state, down, config, events),
              "fresh Q can down dash after contact reset");
        checkNear(state.baseVelocity.z, config.downDashVerticalSpeed, kTolerance,
                  "fresh Q applies down-dash Z after reset");
    }

    {
        MovementState state = defaultState();
        state.jump.airJumpsLeft = 0;
        state.jump.airJumpArmed = false;
        state.jump.airJumpLocked = true;
        state.jump.jumpHeldPreviously = true;
        MovementCommand jump = commandFor(state.lifecycle);
        jump.jumpHeld = true;
        MovementContact contact =
            worldContact(MovementContactKind::Wall, 90, state.lifecycle);
        MovementStepResult result = simulateMovementStepWithSpecials(
            state, jump, config, collisionWith(contact, false), kDt);
        check(result.events.abilitiesReset && result.events.didAirJump,
              "wall contact resets before same-tick held jump");
        checkNear(result.state.baseVelocity.z,
                  movementScaledJumpVelocity(config, result.state.sizeScale),
                  kTolerance,
                  "held Space uses restored air jump once");

        state = result.state;
        contact.simulationTick = 91;
        MovementStepResult next = simulateMovementStepWithSpecials(
            state, jump, config, collisionWith(contact, false), kDt);
        check(next.events.didAirJump,
              "later wall contact can restore and jump again for wall climbing");
    }
}

void testLifecycleFiltering()
{
    const MovementConfig config = testConfig();
    MovementState state = unavailableState();
    MovementStepEvents events;
    MovementContactHistory history;

    MovementContact stale = makeExplosionMovementContact(
        501, 11, 100, life(4, 3), glm::vec3(0), 1.0f);
    MovementContactConsumeResult result =
        consumeMovementContacts(state, config, oneContact(stale), history, events);
    check(!state.dash.dashAvailable && result.ignoredLifecycleCount == 1,
          "old spawn contact ignored before event emission");

    MovementContact accepted = makeExplosionMovementContact(
        501, 11, 101, state.lifecycle, glm::vec3(0), 1.0f);
    consumeMovementContacts(state, config, oneContact(accepted), history, events);
    check(state.dash.dashAvailable, "matching lifecycle accepted");

    state.dash.dashAvailable = false;
    MovementContact wrongEpoch = makeExplosionMovementContact(
        502, 11, 102, life(7, 4), glm::vec3(0), 1.0f);
    events = {};
    result = consumeMovementContacts(state, config, oneContact(wrongEpoch), history, events);
    check(!state.dash.dashAvailable && result.ignoredLifecycleCount == 1,
          "wrong transform epoch ignored");

    state.lifecycle = life(8, 3);
    MovementContact newLifeSameEvent = makeExplosionMovementContact(
        501, 11, 103, state.lifecycle, glm::vec3(0), 1.0f);
    events = {};
    consumeMovementContacts(state, config, oneContact(newLifeSameEvent), history, events);
    check(state.dash.dashAvailable,
          "new lifecycle clears/gates stable event history");
}

void testCapacityOverflow()
{
    const MovementConfig config = testConfig();
    MovementState state = unavailableState();
    MovementContactSet contacts;
    for (std::size_t i = 0; i < MovementContactSet::Capacity; ++i) {
        contacts.addDeduplicated(worldContact(MovementContactKind::OtherGameplay,
                                              120,
                                              state.lifecycle,
                                              glm::vec3(1, 0, 0),
                                              static_cast<uint32_t>(i + 1),
                                              false));
    }
    contacts.addDeduplicated(worldContact(MovementContactKind::Wall,
                                          120,
                                          state.lifecycle,
                                          glm::vec3(1, 0, 0),
                                          999,
                                          true));

    bool keptReset = false;
    for (const MovementContact& contact : contacts)
        keptReset = keptReset || contact.resetsAbilities;
    check(contacts.size() == MovementContactSet::Capacity,
          "contact set stays within fixed capacity");
    check(contacts.overflowCount == 1, "contact set records overflow");
    check(keptReset, "overflow preserves a reset-producing contact");

    MovementStepEvents events;
    MovementContactHistory history;
    consumeMovementContacts(state, config, contacts, history, events);
    check(state.dash.dashAvailable, "retained overflow reset contact is consumed");
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
    MovementState state = defaultState(life(1000 + static_cast<uint32_t>(index), 5));
    state.baseVelocity = glm::vec3(randomFloat(rng, -80.0f, 80.0f),
                                   randomFloat(rng, -80.0f, 80.0f),
                                   randomFloat(rng, -80.0f, 80.0f));
    state.externalImpulse = glm::vec3(randomFloat(rng, -120.0f, 120.0f),
                                      randomFloat(rng, -120.0f, 120.0f),
                                      randomFloat(rng, -120.0f, 120.0f));
    state.jump.airJumpsLeft = randomBool(rng) ? 1 : 0;
    state.jump.airJumpArmed = randomBool(rng);
    state.jump.airJumpLocked = randomBool(rng);
    state.dash.dashAvailable = randomBool(rng);
    state.downDash.available = randomBool(rng);
    state.freeze.available = randomBool(rng);
    state.freeze.active = randomBool(rng);
    state.freeze.timerSeconds = randomFloat(rng, 0.0f, 6.0f);
    state.freeze.heldPreviously = randomBool(rng);
    state.groundReturn.available = randomBool(rng);
    state.dashMomentumProtection.active = randomBool(rng);
    state.dashMomentumProtection.protectedMoveAxes =
        movementClampUnitOrZero(glm::vec2(randomFloat(rng, -1.0f, 1.0f),
                                          randomFloat(rng, -1.0f, 1.0f)));
    return state;
}

MovementContactKind randomKind(std::mt19937& rng)
{
    static constexpr MovementContactKind kinds[] = {
        MovementContactKind::Ground,
        MovementContactKind::Wall,
        MovementContactKind::Ceiling,
        MovementContactKind::Slope,
        MovementContactKind::Step,
        MovementContactKind::Projectile,
        MovementContactKind::Explosion,
        MovementContactKind::OtherGameplay,
    };
    constexpr int kindCount = static_cast<int>(sizeof(kinds) / sizeof(kinds[0]));
    std::uniform_int_distribution<int> dist(0, kindCount - 1);
    return kinds[dist(rng)];
}

void testRandomizedInvariants()
{
    std::mt19937 rng(kRandomSeed);
    const MovementConfig config = testConfig();
    const int failuresBefore = gFailures;

    for (int i = 0; i < kRandomCases; ++i) {
        MovementState state = randomState(rng, i);
        const MovementState before = state;
        MovementContactSet contacts;
        const int contactCount = 1 + (i % 40);
        const uint64_t tick = 1000 + static_cast<uint64_t>(i);

        for (int c = 0; c < contactCount; ++c) {
            MovementContact contact;
            const bool dynamic = randomBool(rng);
            const bool stale = randomBool(rng) && c == 0;
            const MovementLifecycleIdentity target =
                stale ? life(before.lifecycle.spawnGeneration + 1, before.lifecycle.transformEpoch)
                      : before.lifecycle;
            if (dynamic) {
                const uint64_t eventId =
                    randomBool(rng) ? static_cast<uint64_t>(200 + (c % 5)) : 0;
                contact = makeEntityMovementContact(
                    randomKind(rng),
                    MovementContactSource::Projectile,
                    static_cast<uint32_t>(300 + c),
                    0,
                    eventId,
                    tick,
                    target,
                    glm::vec3(randomFloat(rng, -3.0f, 3.0f)),
                    glm::vec3(randomFloat(rng, -1.0f, 1.0f),
                              randomFloat(rng, -1.0f, 1.0f),
                              randomFloat(rng, -1.0f, 1.0f)),
                    randomFloat(rng, 0.0f, 5.0f),
                    randomBool(rng));
            } else {
                contact = worldContact(randomKind(rng),
                                       tick,
                                       target,
                                       glm::vec3(randomFloat(rng, -1.0f, 1.0f),
                                                 randomFloat(rng, -1.0f, 1.0f),
                                                 randomFloat(rng, -1.0f, 1.0f)),
                                       static_cast<uint32_t>(1 + (c % 8)),
                                       randomBool(rng));
            }
            contacts.addDeduplicated(contact);
        }

        MovementStepEvents events;
        MovementContactHistory history;
        MovementContactConsumeResult result =
            consumeMovementContacts(state, config, contacts, history, events);

        std::ostringstream context;
        context << "random contact case=" << i;
        checkVec3(state.baseVelocity, before.baseVelocity,
                  context.str() + " preserves base velocity");
        checkVec3(state.externalImpulse, before.externalImpulse,
                  context.str() + " preserves external impulse");
        check(state.dashMomentumProtection.active == before.dashMomentumProtection.active,
              context.str() + " preserves dash protection");
        check(state.freeze.active == before.freeze.active,
              context.str() + " preserves freeze active flag");
        checkNear(state.freeze.timerSeconds, before.freeze.timerSeconds, kTolerance,
                  context.str() + " preserves freeze timer");
        check(contacts.size() <= MovementContactSet::Capacity,
              context.str() + " bounded contact storage");
        check(movementIsFinite(state), context.str() + " state remains finite");

        if (result.appliedReset && !result.reset.anyRestored)
            check(!events.abilitiesReset,
                  context.str() + " idempotent reset emits no event");
    }

    check(gFailures == failuresBefore,
          "randomized invariants had no mismatches for seed 0x2C2026 across 512 cases");
}

} // namespace

int main()
{
    testCentralReset();
    testMeaningfulEvents();
    testContactClassification();
    testWorldContactResets();
    testMultipleAndStaticDedupe();
    testDynamicStableDedupe();
    testExplosionAndMomentumSeparation();
    testFreezeResetSemantics();
    testDashDownDashAndAirJumpPhase();
    testLifecycleFiltering();
    testCapacityOverflow();
    testRandomizedInvariants();

    if (gFailures != 0) {
        std::cerr << "[movement-contact-reset-test] FAIL failures=" << gFailures
                  << " seed=" << kRandomSeed
                  << " randomizedCases=" << kRandomCases
                  << " capacity=" << MovementContactSet::Capacity
                  << " maxDifference=" << gMaxDifference << "\n";
        return 1;
    }

    std::cout << "[movement-contact-reset-test] PASS seed=" << kRandomSeed
              << " randomizedCases=" << kRandomCases
              << " capacity=" << MovementContactSet::Capacity
              << " maxDifference<=" << gMaxDifference << "\n";
    return 0;
}
